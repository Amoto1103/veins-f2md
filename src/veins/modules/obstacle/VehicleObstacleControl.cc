//
// Copyright (C) 2010-2018 Christoph Sommer <sommer@ccs-labs.org>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include <sstream>
#include <map>
#include <set>

#include <limits>
#include <cmath>

#include "veins/modules/obstacle/VehicleObstacleControl.h"
#include "veins/modules/mobility/traci/TraCIMobility.h"
#include "veins/base/connectionManager/ChannelAccess.h"

using Veins::VehicleObstacle;
using Veins::VehicleObstacleControl;

Define_Module(Veins::VehicleObstacleControl);

VehicleObstacleControl::~VehicleObstacleControl() = default;

void VehicleObstacleControl::initialize(int stage)
{
    if (stage == 1) {
        annotations = AnnotationManagerAccess().getIfExists();
        if (annotations) {
            vehicleAnnotationGroup = annotations->createGroup("vehicleObstacles");
        }
    }
}

void VehicleObstacleControl::finish()
{
}

void VehicleObstacleControl::handleMessage(cMessage* msg)
{
    if (msg->isSelfMessage()) {
        handleSelfMsg(msg);
        return;
    }
    error("VehicleObstacleControl doesn't handle messages from other modules");
}

void VehicleObstacleControl::handleSelfMsg(cMessage* msg)
{
    error("VehicleObstacleControl doesn't handle self-messages");
}

const VehicleObstacle* VehicleObstacleControl::add(VehicleObstacle obstacle)
{
    auto* o = new VehicleObstacle(obstacle);
    vehicleObstacles.push_back(o);

    return o;
}

void VehicleObstacleControl::erase(const VehicleObstacle* obstacle)
{
    bool erasedOne = false;
    for (auto k = vehicleObstacles.begin(); k != vehicleObstacles.end();) {
        VehicleObstacle* o = *k;
        if (o == obstacle) {
            erasedOne = true;
            k = vehicleObstacles.erase(k);
        }
        else {
            ++k;
        }
    }
    ASSERT(erasedOne);
    delete obstacle;
}

namespace {
// Calculate impact of vehicles as obstacles according to:
// M. Boban, T. T. V. Vinhoza, M. Ferreira, J. Barros, and O. K. Tonguz: 'Impact of Vehicles as Obstacles in Vehicular Ad Hoc Networks', IEEE JSAC, Vol. 29, No. 1, January 2011

/**
 * compute attenuation due to (single) vehicle.
 * @param h1: height of sender
 * @param h2: height of receiver
 * @param h: height of obstacle
 * @param d: distance between sender and receiver
 * @param d1: distance between sender and obstacle
 * @param f: frequency of the transmission
 */
double getVehicleAttenuationSingle(double h1, double h2, double h, double d, double d1, double f)
{
    double lambda = 0.3 / f;
    double d2 = d - d1;
    double y = (h2 - h1) / d * d1 + h1;
    double H = h - y;
    double r1 = sqrt(lambda * d1 * d2 / d);
    double V0 = sqrt(2) * H / r1;

    if (V0 <= -0.7) return 0;

    return 6.9 + 20 * log10(sqrt(pow((V0 - 0.1), 2) + 1) + V0 - 0.1);
}

/**
 * compute attenuation due to vehicles.
 * @param dz_vec: a vector of (distance, height) referring to potential obstacles along the line of sight, starting with the sender and ending with the receiver
 * @param f: the frequency of the transmission
 */
double getVehicleAttenuationDZ(const std::vector<std::pair<double, double>>& dz_vec, double f)
{

    // basic sanity check
    assert(dz_vec.size() >= 2);

    // make sure the list of x coordinates is sorted
    for (size_t i = 0; i < dz_vec.size() - 1; i++) {
        assert(dz_vec[i].first < dz_vec[i + 1].first);
    }

    // find "major obstacles" (MOs) between sender and receiver via rope-stretching algorithm
    /*
     *      |
     *      |         |
     *      |   :     |
     *  |   |   :  :  |    |
     * mo0 mo1       mo2  mo3
     * snd                rcv
     */
    std::vector<size_t> mo; ///< indices of MOs (this includes the sender and receiver)
    mo.push_back(0);
    for (size_t i = 0;;) {
        double max_angle = -std::numeric_limits<double>::infinity();
        size_t max_angle_index;

        for (size_t j = i + 1; j < dz_vec.size(); ++j) {
            double angle = (dz_vec[j].second - dz_vec[i].second) / (dz_vec[j].first - dz_vec[i].first);

            if (angle > max_angle) {
                max_angle = angle;
                max_angle_index = j;
            }
        }

        if (max_angle_index >= dz_vec.size() - 1) break;

        mo.push_back(max_angle_index);

        i = max_angle_index;
    }
    mo.push_back(dz_vec.size() - 1);

    // calculate attenuation due to MOs
    double attenuation_mo = 0;
    for (size_t mm = 0; mm < mo.size() - 2; ++mm) {
        size_t tx = mo[mm];
        size_t ob = mo[mm + 1];
        size_t rx = mo[mm + 2];

        double h1 = dz_vec[tx].second;
        double h2 = dz_vec[rx].second;
        double d = dz_vec[rx].first - dz_vec[tx].first;
        double d1 = dz_vec[ob].first - dz_vec[tx].first;
        double h = dz_vec[ob].second;

        double ad_mo = getVehicleAttenuationSingle(h1, h2, h, d, d1, f);

        attenuation_mo += ad_mo;
    }

    // calculate attenuation due to "small obstacles" (i.e. the ones in-between MOs)
    double attenuation_so = 0;
    for (size_t i = 0; i < mo.size() - 1; ++i) {
        size_t delta = mo[i + 1] - mo[i];

        if (delta == 1) {
            // no obstacle in-between these two MOs
        }
        else if (delta == 2) {
            // one obstacle in-between these two MOs
            size_t tx = mo[i];
            size_t ob = mo[i] + 1;
            size_t rx = mo[i + 1];

            double h1 = dz_vec[tx].second;
            double h2 = dz_vec[rx].second;
            double d = dz_vec[rx].first - dz_vec[tx].first;
            double d1 = dz_vec[ob].first - dz_vec[tx].first;
            double h = dz_vec[ob].second;

            double ad_mo = getVehicleAttenuationSingle(h1, h2, h, d, d1, f);
            attenuation_so += ad_mo;
        }
        else {
            // multiple obstacles in-between these two MOs -- use the one closest to their line of sight
            double x1 = dz_vec[mo[i]].first;
            double y1 = dz_vec[mo[i]].second;
            double x2 = dz_vec[mo[i + 1]].first;
            double y2 = dz_vec[mo[i + 1]].second;

            double min_delta_h = std::numeric_limits<float>::infinity();
            size_t min_delta_h_index;
            for (size_t j = mo[i] + 1; j < mo[i + 1]; ++j) {
                double h = (y2 - y1) / (x2 - x1) * (dz_vec[j].first - x1) + y1;
                double delta_h = h - dz_vec[j].second;

                if (delta_h < min_delta_h) {
                    min_delta_h = delta_h;
                    min_delta_h_index = j;
                }
            }

            size_t tx = mo[i];
            size_t ob = min_delta_h_index;
            size_t rx = mo[i + 1];

            double h1 = dz_vec[tx].second;
            double h2 = dz_vec[rx].second;
            double d = dz_vec[rx].first - dz_vec[tx].first;
            double d1 = dz_vec[ob].first - dz_vec[tx].first;
            double h = dz_vec[ob].second;

            double ad_mo = getVehicleAttenuationSingle(h1, h2, h, d, d1, f);
            attenuation_so += ad_mo;
        }
    }

    double c;
    {
        double prodS = 1;
        double sumS = 0;
        double prodSsum = 1;
        double firstS = 0;
        double lastS = 0;

        double s_old = 0;
        for (size_t jj = 0; jj < mo.size() - 1; ++jj) {
            double s = dz_vec[mo[jj + 1]].first - dz_vec[mo[jj]].first; ///< distance between two MOs

            prodS *= s;
            sumS += s;
            if (jj == 0)
                firstS = s;
            else if (jj > 0)
                prodSsum *= (s + s_old);
            if (jj == mo.size() - 2) lastS = s;
            s_old = s;
        }

        c = -10 * log10((prodS * sumS) / (prodSsum * firstS * lastS));
    }

    return attenuation_mo + attenuation_so + c;
}
} // namespace

double VehicleObstacleControl::calculateVehicleAttenuation(const Coord& senderPos, const Coord& receiverPos, const Signal& s) const
{
    Enter_Method_Silent();

    double senderHeight = senderPos.z;
    double receiverHeight = receiverPos.z;

    ASSERT(senderHeight > 0);
    ASSERT(receiverHeight > 0);

    std::vector<std::pair<double, double>> potentialObstacles; /**< linear position of each obstructing vehicle along (senderPos--receiverPos) */

    simtime_t sStart = s.getSendingStart();

    EV << "searching candidates for transmission from " << senderPos.info() << " -> " << receiverPos.info() << " (" << senderPos.distance(receiverPos) << "meters total)" << std::endl;

    if (hasGUI() && annotations) {
        annotations->eraseAll(vehicleAnnotationGroup);
        drawVehicleObstacles(sStart);
        annotations->drawLine(senderPos, receiverPos, "blue", vehicleAnnotationGroup);
    }

    double x1 = std::min(senderPos.x, receiverPos.x);
    double x2 = std::max(senderPos.x, receiverPos.x);
    double y1 = std::min(senderPos.y, receiverPos.y);
    double y2 = std::max(senderPos.y, receiverPos.y);

    for (auto o : vehicleObstacles) {
        auto* ca = o->getChannelAccess();
        Coord p = ca->getAntennaPosition();
        double l = o->getLength();
        double w = o->getWidth();
        double h = o->getHeight();

        EV << "checking vehicle at " << p.info() << " with height: " << h << " width: " << w << " length: " << l << endl;
        // shortcut if AABBs can't overlap
        double lw = std::max(l, w);
        if ((p.x + lw) < x1) continue;
        if ((p.x - lw) > x2) continue;
        if ((p.y + lw) < y1) continue;
        if ((p.y - lw) > y2) continue;

        if (p == senderPos) {
            // this is the sender: ignore
        }
        else if (p == receiverPos) {
            // this is the receiver: ignore
        }
        else {
            // this is a potential obstacle
            double p1d = o->getIntersectionPoint(senderPos, receiverPos);
            double maxd = senderPos.distance(receiverPos);
            if (!std::isnan(p1d) && p1d > 0 && p1d < maxd) {
                auto it = potentialObstacles.begin();
                while (true) {
                    if (it == potentialObstacles.end()) {
                        potentialObstacles.emplace_back(p1d, h);
                        break;
                    }
                    if (it->first == p1d) { // omit double entries
                        EV << "two obstacles at same distance " << it->first << " == " << p1d << " height: " << it->second << " =? " << h << std::endl;
                        break;
                    }
                    if (it->first > p1d) {
                        potentialObstacles.insert(it, std::make_pair(p1d, h));
                        break;
                    }
                    ++it;
                }
                EV << "\tgot obstacle in 2d-LOS at " << p.info() << ", " << p1d << " meters away from sender" << std::endl;
                Coord hitPos = senderPos + (receiverPos - senderPos) / senderPos.distance(receiverPos) * p1d;
                if (hasGUI() && annotations)
                    annotations->drawLine(senderPos, hitPos, "red", vehicleAnnotationGroup);
            }
        }
    }

    if (potentialObstacles.size() < 1) return 1;

    potentialObstacles.insert(potentialObstacles.begin(), std::make_pair(0, senderHeight));
    potentialObstacles.emplace_back(senderPos.distance(receiverPos), receiverHeight);

    double attenuationDB = getVehicleAttenuationDZ(potentialObstacles, carrierFrequency);

    EV << "t=" << simTime() << ": Attenuation by vehicles is " << attenuationDB << " dB" << std::endl;

    return pow(10.0, -attenuationDB / 10.0);
}

void VehicleObstacleControl::drawVehicleObstacles(const simtime_t& t) const
{
    for (auto o : vehicleObstacles) {
        annotations->drawPolygon(o->getShape(), "black", vehicleAnnotationGroup);
    }
}

void VehicleObstacleControl::setCarrierFrequency(const double cf)
{
    carrierFrequency = cf;
}
