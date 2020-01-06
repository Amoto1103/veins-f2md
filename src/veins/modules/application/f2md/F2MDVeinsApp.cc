/*******************************************************************************
 * @author  Joseph Kamel
 * @email   josephekamel@gmail.com
 * @date    28/11/2018
 * @version 2.0
 *
 * SCA (Secure Cooperative Autonomous systems)
 * Copyright (c) 2013, 2018 Institut de Recherche Technologique SystemX
 * All rights reserved.
 *******************************************************************************/

#include <veins/modules/application/f2md/F2MDVeinsApp.h>

Define_Module(JosephVeinsApp);
//Simulation Parameters
#define serialNumber "IRT-DEMO"
#define savePath "../../../../mdmSave/"

#define veremiConf true
#define randomConf false
#define variableConf false
#define confPos 10.0
#define confSpd 0.05
#define confHea 20.0
#define confAcc 0.01
#define confPrec 100.0
#define minConf 0.20 //20%

#define SAVE_PERIOD 10 //60 seconds
#define PRINT_PERIOD 60 //60 seconds

#define START_SAVE 0 //60 seconds
#define START_ATTACK 10 //60 seconds

#define REPORT_VERSION reportTypes::ProtocolReport

static bool UseAttacksServer = false;
static bool MixLocalAttacks = true;
static bool RandomLocalMix = false;
static int LastLocalAttackIndex = -1;
#define LOCAL_ATTACKER_PROB 0.05

#define LOCAL_ATTACK_TYPE attackTypes::RandomSpeed

static attackTypes::Attacks MixLocalAttacksList[] =
        { attackTypes::ConstPos, attackTypes::Disruptive,
                attackTypes::RandomPos, attackTypes::StaleMessages,
                attackTypes::DoSRandomSybil, attackTypes::ConstPosOffset,
                attackTypes::ConstSpeed, attackTypes::DoS,
                attackTypes::RandomPosOffset, attackTypes::DataReplaySybil,
                attackTypes::DoSDisruptive, attackTypes::ConstSpeedOffset,
                attackTypes::RandomSpeedOffset, attackTypes::EventualStop,
                attackTypes::DoSDisruptiveSybil, attackTypes::DataReplay,
                attackTypes::DoSRandom, attackTypes::GridSybil,
                attackTypes::RandomSpeed };

//ConstPos, ConstPosOffset, RandomPos, RandomPosOffset,
//ConstSpeed, ConstSpeedOffset, RandomSpeed, RandomSpeedOffset,
//EventualStop, Disruptive, DataReplay, StaleMessages,
//DoS, DoSRandom, DoSDisruptive,
//GridSybil, DataReplaySybil, DoSRandomSybil, DoSDisruptiveSybil,

#define GLOBAL_ATTACKER_PROB 0.0
#define GLOBAL_ATTACK_TYPE attackTypes::MAStress
// 1 MAStress

static bool EnablePC = false;
#define PC_TYPE pseudoChangeTypes::Car2car
// Periodical, Disposable, DistanceBased, Random, Car2car
//Detection Application

static bool EnableV1 = true;
static bool EnableV2 = true;
static bool SaveStatsV1 = true;
static bool SaveStatsV2 = true;

static mdChecksVersionTypes::ChecksVersion checksVersionV1 =
        mdChecksVersionTypes::ExperiChecks;
static mdChecksVersionTypes::ChecksVersion checksVersionV2 =
        mdChecksVersionTypes::ExperiChecks;

static mdAppTypes::App appTypeV1 = mdAppTypes::ThresholdApp;
static mdAppTypes::App appTypeV2 = mdAppTypes::BehavioralApp;

static bool writeSelfMsg = false;
static bool writeListSelfMsg = false;

//writeBsms
static bool writeBsmsV1 = false;
static bool writeBsmsV2 = false;
static bool writeListBsmsV1 = false;
static bool writeListBsmsV2 = false;

//writeReport
static bool writeReportsV1 = false;
static bool writeReportsV2 = false;
static bool writeListReportsV1 = false;
static bool writeListReportsV2 = false;

//writeReport
static bool writeVeReMi = false;
static double VeReMiSliceTime = 3600;
static double VeReMiSliceStartTime = 0;

static bool sendReportsV1 = false;
static bool sendReportsV2 = false;
static std::string maHostV1 = "localhost";
static std::string maHostV2 = "localhost";
static int maPortV1 = 9980;
static int maPortV2 = 9981;
//static int maPortV2 = 32790;

static bool enableVarThreV1 = false;
static bool enableVarThreV2 = false;
//Simulation Parameters

void JosephVeinsApp::initialize(int stage) {

    BaseWaveApplLayer::initialize(stage);

    if (!linkInit) {
        linkControl.initialize(traci);
        linkInit = true;
    }

    if (stage == 0) {

        // F2MD init
        curPositionConfidenceOrig = Coord(0, 0, 0);
        curSpeedConfidenceOrig = Coord(0, 0, 0);
        curHeadingConfidenceOrig = Coord(0, 0, 0);
        curAccelConfidenceOrig = Coord(0, 0, 0);

        curPositionConfidence = Coord(0, 0, 0);
        curSpeedConfidence = Coord(0, 0, 0);
        curHeadingConfidence = Coord(0, 0, 0);
        curAccelConfidence = Coord(0, 0, 0);
        myWidth = 0;
        myLength = 0;
        curHeading = Coord(0, 0, 0);
        curAccel = Coord(0, 0, 0);
        myMdType = mbTypes::Genuine;
        myAttackType = attackTypes::Attacks::Genuine;
        //attackBsm.setSenderAddress(0);
        //nextAttackBsm.setSenderAddress(0);
        // F2MD init

    } else if (stage == 1) {
        EV << "Initializing " << par("appName").stringValue() << std::endl;

        myVType = traciVehicle->getVType();

        MaxRandomPosX = mobility->getplaygroundSizeX();
        MaxRandomPosY = mobility->getplaygroundSizeY();

        setMDApp(appTypeV1, appTypeV2);

        MAX_PLAUSIBLE_ACCEL = traciVehicle->getAccel() + 0.01;
        MAX_PLAUSIBLE_DECEL = traciVehicle->getDeccel() + 0.01;
        MAX_PLAUSIBLE_SPEED = traciVehicle->getMaxSpeed() + 0.01;

        myWidth = traciVehicle->getWidth();
        myLength = traciVehicle->getLength();

        myMdType = induceMisbehavior(LOCAL_ATTACKER_PROB, GLOBAL_ATTACKER_PROB);

        if (UseAttacksServer) {
            localAttackServer.pingAttackServer();

            if (localAttackServer.isQueuedAttack()) {
                if (localAttackServer.isInstantAttack()) {
                    if (myMdType == mbTypes::Genuine) {
                        myMdType = mbTypes::LocalAttacker;
                    }
                }
            } else {
                if (myMdType == mbTypes::LocalAttacker) {
                    myMdType = mbTypes::Genuine;
                }
            }
        }

        //pseudonym-------------------------------
        myPcType = PC_TYPE;
        pseudoNum = 0;

        pcPolicy = PCPolicy(mobility->getPositionAt(simTime()));

        pcPolicy.setMbType(myMdType);
        pcPolicy.setMdAuthority(&mdStats);
        pcPolicy.setCurPosition(&curPosition);
        pcPolicy.setMyId(&myId);
        pcPolicy.setMyPseudonym(&myPseudonym);
        pcPolicy.setPseudoNum(&pseudoNum);

        myPseudonym = pcPolicy.getNextPseudonym();

        //pseudonym-------------------------------

        if (randomConf) {
            double randConfPos = genLib.RandomDouble(confPos * minConf,
            confPos);
            double randConfSpeed = genLib.RandomDouble(confSpd * minConf,
            confSpd);
            double randConfHeading = genLib.RandomDouble(confHea * minConf,
            confHea);
            double randConfAccel = genLib.RandomDouble(confAcc * minConf,
            confAcc);

            if (variableConf) {
                ConfPosMax = Coord(randConfPos * minConf, randConfPos * minConf,
                        0);
                ConfSpeedMax = Coord(randConfSpeed * minConf,
                        randConfSpeed * minConf, 0);
                ConfHeadMax = Coord(randConfHeading * minConf,
                        randConfHeading * minConf, 0);
                ConfAccelMax = Coord(randConfAccel * minConf,
                        randConfAccel * minConf, 0);
            } else {
                ConfPosMax = Coord(0.0, 0.0, 0.0);
                ConfSpeedMax = Coord(0.0, 0.0, 0.0);
                ConfHeadMax = Coord(0.0, 0.0, 0.0);
                ConfAccelMax = Coord(0.0, 0.0, 0.0);
            }

            randConfPos = ((int) (randConfPos * confPrec + .5) / confPrec);
            randConfSpeed = ((int) (randConfSpeed * confPrec + .5) / confPrec);
            randConfHeading = ((int) (randConfHeading * confPrec + .5)
                    / confPrec);
            randConfAccel = ((int) (randConfAccel * confPrec + .5) / confPrec);

            curPositionConfidenceOrig = Coord(randConfPos, randConfPos, 0);
            curSpeedConfidenceOrig = Coord(randConfSpeed, randConfSpeed, 0);
            curHeadingConfidenceOrig = Coord(randConfHeading, randConfHeading,
                    0);
            curAccelConfidenceOrig = Coord(randConfAccel, randConfAccel, 0);

            curPositionConfidence = Coord(randConfPos, randConfPos, 0);
            curSpeedConfidence = Coord(randConfSpeed, randConfSpeed, 0);
            curHeadingConfidence = Coord(randConfHeading, randConfHeading, 0);
            curAccelConfidence = Coord(randConfAccel, randConfAccel, 0);

        } else if (veremiConf) {
            double posE0 = genLib.RandomDouble(3, 5);
            double speedE0 = genLib.GaussianRandomDouble(0, 0.0016);
            double headingE0 = genLib.RandomDouble(0, 20);

            curPositionConfidenceOrig = Coord(posE0, posE0, 0);
            curSpeedConfidenceOrig = Coord(speedE0, speedE0, 0);
            curHeadingConfidenceOrig = Coord(headingE0, headingE0, 0);
            curAccelConfidenceOrig = Coord(0, 0, 0);

            curPositionConfidence = Coord(posE0, posE0, 0);
            curSpeedConfidence = Coord(speedE0, speedE0, 0);
            curHeadingConfidence = Coord(headingE0, headingE0, 0);
            curAccelConfidence = Coord(0, 0, 0);

            lastPositionUpdate = simTime().dbl()-1.0;
        } else {
            if (variableConf) {
                ConfPosMax = Coord(confPos * minConf, confPos * minConf, 0);
                ConfSpeedMax = Coord(confSpd * minConf, confSpd * minConf, 0);
                ConfHeadMax = Coord(confHea * minConf, confHea * minConf, 0);
                ConfAccelMax = Coord(confAcc * minConf, confAcc * minConf, 0);
            } else {
                ConfPosMax = Coord(0.0, 0.0, 0.0);
                ConfSpeedMax = Coord(0.0, 0.0, 0.0);
                ConfHeadMax = Coord(0.0, 0.0, 0.0);
                ConfAccelMax = Coord(0.0, 0.0, 0.0);
            }

            double ConfPosR = ((int) (confPos * confPrec + .5) / confPrec);
            double ConfSpeedR = ((int) (confSpd * confPrec + .5) / confPrec);
            double ConfHeadingR = ((int) (confHea * confPrec + .5) / confPrec);
            double ConfAccelR = ((int) (confAcc * confPrec + .5) / confPrec);

            curPositionConfidenceOrig = Coord(ConfPosR, ConfPosR, 0);
            curSpeedConfidenceOrig = Coord(ConfSpeedR, ConfSpeedR, 0);
            curHeadingConfidenceOrig = Coord(ConfHeadingR, ConfHeadingR, 0);
            curAccelConfidenceOrig = Coord(ConfAccelR, ConfAccelR, 0);

            curPositionConfidence = Coord(ConfPosR, ConfPosR, 0);
            curSpeedConfidence = Coord(ConfSpeedR, ConfSpeedR, 0);
            curHeadingConfidence = Coord(ConfHeadingR, ConfHeadingR, 0);
            curAccelConfidence = Coord(ConfAccelR, ConfAccelR, 0);
        }

        myReportType = REPORT_VERSION;

        switch (myMdType) {
        case mbTypes::Genuine: {
            traciVehicle->setColor(TraCIColor(0, 255, 0, 255));
            myAttackType = attackTypes::Genuine;
        }
            break;
        case mbTypes::GlobalAttacker: {
            traciVehicle->setColor(TraCIColor(0, 255, 0, 255));
            myAttackType = GLOBAL_ATTACK_TYPE;

            mdGlobalAttack = MDGlobalAttack();

            mdGlobalAttack.setMyPseudonym(&myPseudonym);
            mdGlobalAttack.setCurHeading(&curHeading);
            mdGlobalAttack.setCurHeadingConfidence(&curHeadingConfidence);
            mdGlobalAttack.setCurPosition(&curPosition);
            mdGlobalAttack.setCurPositionConfidence(&curPositionConfidence);
            mdGlobalAttack.setCurSpeed(&curSpeed);
            mdGlobalAttack.setCurSpeedConfidence(&curSpeedConfidence);
            mdGlobalAttack.setCurAccel(&curAccel);
            mdGlobalAttack.setCurAccelConfidence(&curAccelConfidence);
            mdGlobalAttack.setTraci(traci);

            mdGlobalAttack.init(myAttackType);

        }
            break;
        case mbTypes::LocalAttacker: {
            //attack-------------------------------
            if (UseAttacksServer) {
                myAttackType = localAttackServer.getNextAttack();
            } else if (MixLocalAttacks) {
                int AtLiSize = sizeof(MixLocalAttacksList)
                        / sizeof(MixLocalAttacksList[0]);
                int localAttackIndex = 0;
                if (RandomLocalMix) {
                    localAttackIndex = genLib.RandomInt(0, AtLiSize - 1);
                } else {
                    if (LastLocalAttackIndex < (AtLiSize - 1)) {
                        localAttackIndex = LastLocalAttackIndex + 1;
                        LastLocalAttackIndex = localAttackIndex;
                    } else {
                        localAttackIndex = 0;
                        LastLocalAttackIndex = 0;
                    }
                }
                myAttackType = MixLocalAttacksList[localAttackIndex];
            } else {
                myAttackType = LOCAL_ATTACK_TYPE;
            }

            std::cout
            << "=+#=+#=+#=+#=+#=+#=+#=+#+#=+#=+#=+#=+#=+#=+#=+#=+#=+#=+#=+#=+# "<<"\n";
            std::cout
            << "=+#=+#=+#=+#=+#=+#=+#=+# NEW ATTACKER =+#=+#=+#=+#=+#=+#=+#=+# "
            << myPseudonym << " : " <<attackTypes::AttackNames[myAttackType]<<"\n";
            std::cout << "=+#=+#=+#=+#=+#=+#=+#=+#+#=+#=+#=+#=+#=+#=+#=+#=+#=+#=+#=+#=+# "<<"\n";

            mdAttack = MDAttack();

            mdAttack.setBeaconInterval(&beaconInterval);
            mdAttack.setCurHeading(&curHeading);
            mdAttack.setCurHeadingConfidence(&curHeadingConfidence);
            mdAttack.setCurPosition(&curPosition);
            mdAttack.setCurPositionConfidence(&curPositionConfidence);
            mdAttack.setCurSpeed(&curSpeed);
            mdAttack.setCurSpeedConfidence(&curSpeedConfidence);
            mdAttack.setCurAccel(&curAccel);
            mdAttack.setCurAccelConfidence(&curAccelConfidence);
            mdAttack.setDetectedNodes(&detectedNodes);
            mdAttack.setMyBsm(myBsm);
            mdAttack.setMyBsmNum(&myBsmNum);
            mdAttack.setMyLength(&myLength);
            mdAttack.setMyPseudonym(&myPseudonym);
            mdAttack.setMyWidth(&myWidth);
            mdAttack.setPcPolicy(&pcPolicy);

            mdAttack.init(myAttackType,MaxRandomPosX,MaxRandomPosY);

            //attack-------------------------------
            traciVehicle->setColor(TraCIColor(255, 0, 0, 255));

        }
        break;
        default:
        traciVehicle->setColor(TraCIColor(0, 0, 0, 0));
        break;
    }

        if (!setDate) {
            char dateBuffer[50];
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            sprintf(dateBuffer, "%d-%d-%d_%d:%d:%d", tm.tm_year + 1900,
                    tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min,
                    tm.tm_sec);
            std::string curDateTemp(dateBuffer);
            curDate = curDateTemp;
            setDate = true;
        }

        if (writeVeReMi) {
            VeReMi.initVeReMiPrintable(savePath, serialNumber, myId,
                    getParentModule()->getId(), myAttackType, curDate,
                    VeReMiSliceTime, VeReMiSliceStartTime, simTime().dbl());
        }

    }
}

void JosephVeinsApp::finish() {

    if (myReportType == reportTypes::ProtocolReport) {
        handleReportProtocol(true);
    }

    BaseWaveApplLayer::finish();

    //statistics recording goes here
}

void JosephVeinsApp::setMDApp(mdAppTypes::App appTypeV1,
        mdAppTypes::App appTypeV2) {
    switch (appTypeV1) {
    case mdAppTypes::ThresholdApp:
        AppV1 = &ThreV1;
        break;
    case mdAppTypes::AggrigationApp:
        AppV1 = &AggrV1;
        break;
    case mdAppTypes::BehavioralApp:
        AppV1 = &BehaV1;
        break;
    case mdAppTypes::CooperativeApp:
        AppV1 = &CoopV1;
        break;
    case mdAppTypes::ExperiApp:
        AppV1 = &ExperV1;
        break;
    case mdAppTypes::PyBridgeApp:
        AppV1 = &PybgV1;
        PybgV1.setMyId(myId);
        break;
    default:
        AppV1 = &ThreV1;
        break;
    }
    switch (appTypeV2) {
    case mdAppTypes::ThresholdApp:
        AppV2 = &ThreV2;
        break;
    case mdAppTypes::AggrigationApp:
        AppV2 = &AggrV2;
        break;
    case mdAppTypes::BehavioralApp:
        AppV2 = &BehaV2;
        break;
    case mdAppTypes::CooperativeApp:
        AppV2 = &CoopV2;
        break;
    case mdAppTypes::ExperiApp:
        AppV2 = &ExperV2;
        break;
    case mdAppTypes::PyBridgeApp:
        AppV2 = &PybgV2;
        PybgV2.setMyId(myId);
        break;
    default:
        AppV2 = &ThreV2;
        break;
    }
}

static double totalGenuine = 0;
static double totalLocalAttacker = 0;
static double totalGlobalAttacker = 0;
mbTypes::Mbs JosephVeinsApp::induceMisbehavior(double localAttacker,
        double globalAttacker) {

    if (simTime().dbl() < START_ATTACK) {
        return mbTypes::Genuine;
    }

    if ((totalLocalAttacker + totalGenuine) == 0) {
        totalGenuine++;
        return mbTypes::Genuine;
    }

    double realFactor = totalLocalAttacker
            / (totalGenuine + totalLocalAttacker);
    if (localAttacker > realFactor) {
        totalLocalAttacker++;
        return mbTypes::LocalAttacker;
    } else {
        double realGFactor = totalGlobalAttacker
                / (totalGenuine + totalGlobalAttacker);
        if (globalAttacker > realGFactor) {
            totalGlobalAttacker++;
            return mbTypes::GlobalAttacker;
        } else {
            totalGenuine++;
            return mbTypes::Genuine;
        }
    }
}

void JosephVeinsApp::onBSM(BasicSafetyMessage* bsm) {

    if (writeVeReMi) {
        VeReMi.serializeBeacon(bsm);
    }

    unsigned long senderPseudonym = bsm->getSenderPseudonym();

    if (EnableV1) {
        LocalMisbehaviorDetection(bsm, 1);
    }

    if (EnableV2) {
        LocalMisbehaviorDetection(bsm, 2);
    }

    if (!detectedNodes.includes(senderPseudonym)) {
        NodeHistory newNode(senderPseudonym);
        newNode.addBSM(*bsm);
        MDMHistory newMDM(senderPseudonym);
        if (EnableV1) {
            newMDM.addBsmCheck(bsmCheckV1, 1);
            newMDM.initKalman(bsm, 1);
        }
        if (EnableV2) {
            newMDM.addBsmCheck(bsmCheckV2, 2);
            newMDM.initKalman(bsm, 2);
        }
        detectedNodes.put(senderPseudonym, newNode, newMDM,
                &reportProtocolEnforcerV1, &reportProtocolEnforcerV2);
    } else {
        NodeHistory * existingNode = detectedNodes.getNodeHistoryAddr(
                senderPseudonym);
        existingNode->addBSM(*bsm);
        MDMHistory * existingMDM = detectedNodes.getMDMHistoryAddr(
                senderPseudonym);
        if (EnableV1) {
            existingMDM->addBsmCheck(bsmCheckV1, 1);
        }
        if (EnableV2) {
            existingMDM->addBsmCheck(bsmCheckV2, 2);
        }
        //detectedNodes.put(senderPseudonym, *existingNode, *existingMDM);
    }

//Your application has received a beacon message from another car or RSU
//code for handling the message goes here
}
void JosephVeinsApp::treatAttackFlags() {

    if (myMdType == mbTypes::LocalAttacker) {
        attackBsm = mdAttack.launchAttack(myAttackType, &linkControl);

        if (mdAttack.getTargetNode() >= 0) {
            addTargetNode(mdAttack.getTargetNode());
        }

        if (isAccusedNode(myPseudonym)) {
            traciVehicle->setColor(TraCIColor(0, 0, 0, 255));
        } else {
            traciVehicle->setColor(TraCIColor(255, 0, 0, 255));
        }

    } else {
        if (isTargetNode(myPseudonym)) {
            traciVehicle->setColor(TraCIColor(255, 255, 0, 255));
        } else {
            traciVehicle->setColor(TraCIColor(0, 255, 0, 255));
        }
        if (isAccusedNode(myPseudonym)) {
            traciVehicle->setColor(TraCIColor(0, 0, 255, 255));
        }
    }

    if ((simTime().dbl() - targetClearTime) > MAX_TARGET_TIME) {
        targetClearTime = simTime().dbl();
        clearTargetNodes();
    }

    if ((simTime().dbl() - accusedClearTime) > MAX_ACCUSED_TIME) {
        accusedClearTime = simTime().dbl();
        clearAccusedNodes();
    }

}

void JosephVeinsApp::LocalMisbehaviorDetection(BasicSafetyMessage* bsm,
        int version) {

    unsigned long senderPseudo = bsm->getSenderPseudonym();
    bsm->getArrivalTime();

    switch (version) {
    case 1: {
        std::string mdv = "V1";
        switch (checksVersionV1) {

        case mdChecksVersionTypes::LegacyChecks: {
            LegacyChecks mdm(version, myPseudonym, curPosition, curSpeed,
                    curHeading, Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV1 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        case mdChecksVersionTypes::CatchChecks: {
            CaTChChecks mdm(version, myPseudonym, curPosition,
                    curPositionConfidence, curHeading, curHeadingConfidence,
                    Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV1 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        case mdChecksVersionTypes::ExperiChecks: {
            ExperiChecks mdm(version, myPseudonym, curPosition,
                    curPositionConfidence, curHeading, curHeadingConfidence,
                    Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV1 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        default: {
            LegacyChecks mdm(version, myPseudonym, curPosition, curSpeed,
                    curHeading, Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV1 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        }

        bool result = false;
        auto startV1 = high_resolution_clock::now();
        result = AppV1->CheckNodeForReport(myPseudonym, bsm, &bsmCheckV1,
                &detectedNodes);
        auto endV1 = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(endV1 - startV1);
        meanTimeV1 = ((double) numTimeV1 * meanTimeV1 + (duration.count()))
                / ((double) numTimeV1 + 1.0);
        numTimeV1 = numTimeV1 + 1;

        if (enableVarThreV1) {
            varThrePrintableV1.registerMessage(
                    mbTypes::intMbs[bsm->getSenderMbType()],
                    AppV1->getMinFactor());
        }

        if (result && (myMdType == mbTypes::Genuine)) {
            MDReport reportBase;
            reportBase.setGenerationTime(simTime().dbl());
            reportBase.setSenderPseudo(myPseudonym);
            reportBase.setReportedPseudo(senderPseudo);

            reportBase.setSenderRealId(myId);
            reportBase.setReportedRealId(bsm->getSenderRealId());

            reportBase.setMbType(mbTypes::mbNames[bsm->getSenderMbType()]);
            reportBase.setAttackType(
                    attackTypes::AttackNames[bsm->getSenderAttackType()]);
            std::pair<double, double> currLonLat = traci->getLonLat(
                    curPosition);
            reportBase.setSenderGps(Coord(currLonLat.first, currLonLat.second));
            reportBase.setReportedGps(bsm->getSenderGpsCoordinates());

            char nameV1[32] = "mdaV1";
            mdStats.getReport(nameV1, reportBase);
            if (writeReportsV1 && myBsmNum > 0) {
                writeReport(reportBase, version, mdv, bsmCheckV1, bsm);
            }

            if (writeListReportsV1 && myBsmNum > 0) {
                writeListReport(reportBase, version, mdv, bsmCheckV1, bsm);
            }

            if (sendReportsV1 && myBsmNum > 0) {
                sendReport(reportBase, version, mdv, bsmCheckV1, bsm);
            }
        } else if (myMdType == mbTypes::GlobalAttacker) {
            MDReport reportBase = mdGlobalAttack.launchAttack(myAttackType,
                    bsm);

            if (writeReportsV1) {
                writeReport(reportBase, version, mdv, bsmCheckV1, bsm);
            }

            if (writeListReportsV1) {
                writeListReport(reportBase, version, mdv, bsmCheckV1, bsm);
            }

            if (sendReportsV1) {
                sendReport(reportBase, version, mdv, bsmCheckV1, bsm);
            }
        }

        if (writeBsmsV1) {
            writeMdBsm(mdv, bsmCheckV1, bsm);
        }
        if (writeListBsmsV1) {
            writeMdListBsm(mdv, bsmCheckV1, bsm);
        }

        if (!initV1) {
            AppV1->resetAllFlags();
            //mdAuthority.resetAll();
            initV1 = true;
        }

        if ((simTime().dbl() - deltaTV1) > SAVE_PERIOD) {
            bool printOut = false;
            if ((simTime().dbl() - deltaTVS1) > PRINT_PERIOD) {
                deltaTVS1 = simTime().dbl();
                printOut = true;
                std::cout << "-_-_-_-_-_-_-_-_-_-_-_-_-" << " meanTimeV1:"
                        << meanTimeV1 << " μs " << numTimeV1 << "\n";
            }

            deltaTV1 = simTime().dbl();

            if ((simTime().dbl() > START_SAVE) && SaveStatsV1) {
                AppV1->saveLine(savePath, serialNumber,
                        mobility->getManager()->getManagedHosts().size(),
                        deltaTV1, printOut);

                mdStats.saveLine(savePath, serialNumber, deltaTV1, printOut);
                if (enableVarThreV1) {
                    varThrePrintableV1.saveFile(savePath, serialNumber,
                            printOut);
                }

            }
            AppV1->resetInstFlags();
        }

        if (result) {
            addAccusedNode(senderPseudo);
        }

        break;
    }
    case 2: {

        std::string mdv = "V2";

        switch (checksVersionV2) {
        case mdChecksVersionTypes::LegacyChecks: {
            LegacyChecks mdm(version, myPseudonym, curPosition, curSpeed,
                    curHeading, Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV2 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        case mdChecksVersionTypes::CatchChecks: {
            CaTChChecks mdm(version, myPseudonym, curPosition,
                    curPositionConfidence, curHeading, curHeadingConfidence,
                    Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV2 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        case mdChecksVersionTypes::ExperiChecks: {
            ExperiChecks mdm(version, myPseudonym, curPosition,
                    curPositionConfidence, curHeading, curHeadingConfidence,
                    Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV2 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        default: {
            LegacyChecks mdm(version, myPseudonym, curPosition, curSpeed,
                    curHeading, Coord(myWidth, myLength),
                    Coord(MAX_PLAUSIBLE_SPEED, MAX_PLAUSIBLE_ACCEL,
                            MAX_PLAUSIBLE_DECEL), &linkControl);
            bsmCheckV2 = mdm.CheckBSM(bsm, &detectedNodes);
        }
            break;
        }

        bool result = false;
        auto startV2 = high_resolution_clock::now();
        result = AppV2->CheckNodeForReport(myPseudonym, bsm, &bsmCheckV2,
                &detectedNodes);
        auto endV2 = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(endV2 - startV2);
        meanTimeV2 = ((double) numTimeV2 * meanTimeV2 + (duration.count()))
                / ((double) numTimeV2 + 1.0);
        numTimeV2 = numTimeV2 + 1;

        if (enableVarThreV2) {
            varThrePrintableV2.registerMessage(
                    mbTypes::intMbs[bsm->getSenderMbType()],
                    AppV2->getMinFactor());
        }

        if (result && (myMdType == mbTypes::Genuine)) {
            MDReport reportBase;
            reportBase.setGenerationTime(simTime().dbl());
            reportBase.setSenderPseudo(myPseudonym);
            reportBase.setReportedPseudo(senderPseudo);

            reportBase.setSenderRealId(myId);
            reportBase.setReportedRealId(bsm->getSenderRealId());

            reportBase.setMbType(mbTypes::mbNames[bsm->getSenderMbType()]);
            reportBase.setAttackType(
                    attackTypes::AttackNames[bsm->getSenderAttackType()]);
            std::pair<double, double> currLonLat = traci->getLonLat(
                    curPosition);
            reportBase.setSenderGps(Coord(currLonLat.first, currLonLat.second));
            reportBase.setReportedGps(bsm->getSenderGpsCoordinates());

            char nameV2[32] = "mdaV2";
            mdStats.getReport(nameV2, reportBase);

            if (writeReportsV2 && myBsmNum > 0) {
                writeReport(reportBase, version, mdv, bsmCheckV2, bsm);
            }

            if (writeListReportsV2 && myBsmNum > 0) {
                writeListReport(reportBase, version, mdv, bsmCheckV2, bsm);
            }

            if (sendReportsV2 && myBsmNum > 0) {
                sendReport(reportBase, version, mdv, bsmCheckV2, bsm);
            }

        } else if (myMdType == mbTypes::GlobalAttacker) {
            MDReport reportBase = mdGlobalAttack.launchAttack(myAttackType,
                    bsm);

            if (writeReportsV2) {
                writeReport(reportBase, version, mdv, bsmCheckV2, bsm);
            }

            if (writeListReportsV2) {
                writeListReport(reportBase, version, mdv, bsmCheckV2, bsm);
            }

            if (sendReportsV2) {
                sendReport(reportBase, version, mdv, bsmCheckV2, bsm);
            }

        }

        if (writeBsmsV2) {
            writeMdBsm(mdv, bsmCheckV2, bsm);
        }

        if (writeListBsmsV2) {
            writeMdListBsm(mdv, bsmCheckV2, bsm);
        }

        if (!initV2) {
            AppV2->resetAllFlags();
            //mdAuthority.resetAll();
            initV2 = true;
        }

        if ((simTime().dbl() - deltaTV2) > SAVE_PERIOD) {
            bool printOut = false;
            if ((simTime().dbl() - deltaTVS2) > PRINT_PERIOD) {
                deltaTVS2 = simTime().dbl();
                printOut = true;
                std::cout << "-_-_-_-_-_-_-_-_-_-_-_-_-" << " meanTimeV2:"
                        << meanTimeV2 << " μs " << numTimeV2 << "\n";
            }

            deltaTV2 = simTime().dbl();

            if ((simTime().dbl() > START_SAVE) && SaveStatsV2) {
                AppV2->saveLine(savePath, serialNumber,
                        mobility->getManager()->getManagedHosts().size(),
                        deltaTV2, printOut);

                mdStats.saveLine(savePath, serialNumber, deltaTV2, printOut);
                if (enableVarThreV2) {
                    varThrePrintableV2.saveFile(savePath, serialNumber,
                            printOut);
                }

            }
            AppV2->resetInstFlags();
        }

        if (result) {
            addAccusedNode(senderPseudo);
        }

        break;
    }

    default:
        break;
    }

}

void JosephVeinsApp::writeReport(MDReport reportBase, int version,
        std::string maversion, BsmCheck bsmCheck, BasicSafetyMessage *bsm) {
    switch (myReportType) {
    case reportTypes::BasicCheckReport: {
        BasicCheckReport bcr = BasicCheckReport(reportBase);
        bcr.setReportedCheck(bsmCheck);
        bcr.writeStrToFile(savePath, serialNumber, maversion,
                bcr.getReportPrintableJson(), curDate);
    }
        break;

    case reportTypes::OneMessageReport: {
        OneMessageReport omr = OneMessageReport(reportBase);
        omr.setReportedBsm(*bsm);
        omr.setReportedCheck(bsmCheck);
        omr.writeStrToFile(savePath, serialNumber, maversion,
                omr.getReportPrintableJson(), curDate);
    }
        break;
    case reportTypes::EvidenceReport: {
        EvidenceReport evr = EvidenceReport(reportBase);
        if (myBsmNum > 0) {
            evr.addEvidence(myBsm[0], bsmCheck, *bsm, &detectedNodes);
            evr.writeStrToFile(savePath, serialNumber, maversion,
                    evr.getReportPrintableJson(), curDate);
        } else {
            OneMessageReport omr = OneMessageReport(reportBase);
            omr.setReportedBsm(*bsm);
            omr.setReportedCheck(bsmCheck);
            omr.writeStrToFile(savePath, serialNumber, maversion,
                    omr.getReportPrintableJson(), curDate);
        }
    }
        break;
    case reportTypes::ProtocolReport: {
        bool newNode = false;
        switch (version) {
        case 1: {
            newNode = reportProtocolEnforcerV1.addMisbehavingPseudo(
                    bsm->getSenderPseudonym(), simTime().dbl());
        }
            break;
        case 2: {
            newNode = reportProtocolEnforcerV2.addMisbehavingPseudo(
                    bsm->getSenderPseudonym(), simTime().dbl());
        }
            break;
        }
        if (newNode) {
            ProtocolReport hir = ProtocolReport(reportBase);
            hir.addEvidence(myBsm[0], bsmCheck, *bsm, &detectedNodes,
                    simTime().dbl(),
                    InitialHistory, version);
            hir.writeStrToFile(savePath, serialNumber, maversion,
                    hir.getReportPrintableJson(), curDate);
        }
    }
        break;
    default:
        break;
    }
}

void JosephVeinsApp::writeListReport(MDReport reportBase, int version,
        std::string maversion, BsmCheck bsmCheck, BasicSafetyMessage *bsm) {

    switch (myReportType) {
    case reportTypes::BasicCheckReport: {
        BasicCheckReport bcr = BasicCheckReport(reportBase);
        bcr.setReportedCheck(bsmCheck);
        bcr.writeStrToFileList(savePath, serialNumber, maversion,
                bcr.getReportPrintableJson(), curDate);
    }
        break;

    case reportTypes::OneMessageReport: {
        OneMessageReport omr = OneMessageReport(reportBase);
        omr.setReportedBsm(*bsm);
        omr.setReportedCheck(bsmCheck);
        omr.writeStrToFileList(savePath, serialNumber, maversion,
                omr.getReportPrintableJson(), curDate);
    }
        break;
    case reportTypes::EvidenceReport: {
        EvidenceReport evr = EvidenceReport(reportBase);
        if (myBsmNum > 0) {
            evr.addEvidence(myBsm[0], bsmCheck, *bsm, &detectedNodes);
            evr.writeStrToFileList(savePath, serialNumber, maversion,
                    evr.getReportPrintableJson(), curDate);
        } else {
            OneMessageReport omr = OneMessageReport(reportBase);
            omr.setReportedBsm(*bsm);
            omr.setReportedCheck(bsmCheck);
            omr.writeStrToFileList(savePath, serialNumber, maversion,
                    omr.getReportPrintableJson(), curDate);
        }
    }
        break;
    case reportTypes::ProtocolReport: {
        bool newNode = false;
        switch (version) {
        case 1: {
            newNode = reportProtocolEnforcerV1.addMisbehavingPseudo(
                    bsm->getSenderPseudonym(), simTime().dbl());
        }
            break;
        case 2:
            newNode = reportProtocolEnforcerV2.addMisbehavingPseudo(
                    bsm->getSenderPseudonym(), simTime().dbl());
            break;
        }
        if (newNode) {
            ProtocolReport hir = ProtocolReport(reportBase);
            hir.addEvidence(myBsm[0], bsmCheck, *bsm, &detectedNodes,
                    simTime().dbl(),
                    InitialHistory, version);
            hir.writeStrToFileList(savePath, serialNumber, maversion,
                    hir.getReportPrintableJson(), curDate);
        }
    }
        break;
    default:
        break;
    }

}

void JosephVeinsApp::sendReport(MDReport reportBase, int version,
        std::string maversion, BsmCheck bsmCheck, BasicSafetyMessage *bsm) {

    std::string reportStr = "";

    switch (myReportType) {
    case reportTypes::BasicCheckReport: {
        BasicCheckReport bcr = BasicCheckReport(reportBase);
        bcr.setReportedCheck(bsmCheck);
        reportStr = bcr.getReportPrintableJson();
    }
        break;

    case reportTypes::OneMessageReport: {
        OneMessageReport omr = OneMessageReport(reportBase);
        omr.setReportedBsm(*bsm);
        omr.setReportedCheck(bsmCheck);
        reportStr = omr.getReportPrintableJson();
    }
        break;
    case reportTypes::EvidenceReport: {
        EvidenceReport evr = EvidenceReport(reportBase);
        if (myBsmNum > 0) {
            evr.addEvidence(myBsm[0], bsmCheck, *bsm, &detectedNodes);
            reportStr = evr.getReportPrintableJson();
        } else {
            OneMessageReport omr = OneMessageReport(reportBase);
            omr.setReportedBsm(*bsm);
            omr.setReportedCheck(bsmCheck);
            reportStr = omr.getReportPrintableJson();
        }
    }
        break;
    case reportTypes::ProtocolReport: {
        bool newNode = false;
        switch (version) {
        case 1: {

            newNode = reportProtocolEnforcerV1.addMisbehavingPseudo(
                    bsm->getSenderPseudonym(), simTime().dbl());
        }
            break;
        case 2:

            newNode = reportProtocolEnforcerV2.addMisbehavingPseudo(
                    bsm->getSenderPseudonym(), simTime().dbl());
            break;
        }

        if (newNode) {
            ProtocolReport hir = ProtocolReport(reportBase);
            hir.addEvidence(myBsm[0], bsmCheck, *bsm, &detectedNodes,
                    simTime().dbl(),
                    InitialHistory, version);
            reportStr = hir.getReportPrintableJson();

        } else {
            //do not report now
            return;
        }
    }
        break;
    default:
        reportStr = "ERROR myReportType";
        break;
    }

    //std::cout << reportStr << "\n";
    //exit(0);

    if (!maversion.compare("V1")) {
        HTTPRequest httpr = HTTPRequest(maPortV1, maHostV1);
        std::string response = httpr.Request(reportStr);
    }

    if (!maversion.compare("V2")) {
        HTTPRequest httpr = HTTPRequest(maPortV2, maHostV2);
        std::string response = httpr.Request(reportStr);
    }

}

void JosephVeinsApp::writeMdBsm(std::string version, BsmCheck bsmCheck,
        BasicSafetyMessage *bsm) {
    BsmPrintable bsmPrint = BsmPrintable();
    bsmPrint.setReceiverId(myId);
    bsmPrint.setReceiverPseudo(myPseudonym);
    bsmPrint.setBsm(*bsm);
    bsmPrint.setBsmCheck(bsmCheck);
    bsmPrint.writeStrToFile(savePath, serialNumber, version,
            bsmPrint.getBsmPrintableJson(), curDate);
}

void JosephVeinsApp::writeMdListBsm(std::string version, BsmCheck bsmCheck,
        BasicSafetyMessage *bsm) {
    BsmPrintable bsmPrint = BsmPrintable();
    bsmPrint.setReceiverId(myId);
    bsmPrint.setReceiverPseudo(myPseudonym);
    bsmPrint.setBsm(*bsm);
    bsmPrint.setBsmCheck(bsmCheck);
    bsmPrint.writeStrToFileList(savePath, serialNumber, version,
            bsmPrint.getBsmPrintableJson(), curDate);
}

void JosephVeinsApp::writeSelfBsm(BasicSafetyMessage bsm) {
    BsmPrintable bsmPrint = BsmPrintable();
    bsmPrint.setReceiverId(myId);
    bsmPrint.setReceiverPseudo(myPseudonym);
    bsmPrint.setBsm(bsm);
    bsmPrint.writeSelfStrToFile(savePath, serialNumber,
            bsmPrint.getSelfBsmPrintableJson(myVType), curDate);
}

void JosephVeinsApp::writeSelfListBsm(BasicSafetyMessage bsm) {
    BsmPrintable bsmPrint = BsmPrintable();
    bsmPrint.setReceiverId(myId);
    bsmPrint.setReceiverPseudo(myPseudonym);
    bsmPrint.setBsm(bsm);
    bsmPrint.writeSelfStrToFileList(savePath, serialNumber,
            bsmPrint.getSelfBsmPrintableJson(myVType), curDate);
}

void JosephVeinsApp::onWSM(BaseFrame1609_4* wsm) {
//Your application has received a data message from another car or RSU
//code for handling the message goes here, see TraciDemo11p.cc for examples

}

void JosephVeinsApp::onWSA(DemoServiceAdvertisment* wsa) {
//Your application has received a service advertisement from another car or RSU
//code for handling the message goes here, see TraciDemo11p.cc for examples

}

void JosephVeinsApp::handleSelfMsg(cMessage* msg) {

    treatAttackFlags();

    BaseWaveApplLayer::handleSelfMsg(msg);

    if (sendReportsV1) {
        std::string reportStr = "curTime:";
        reportStr.append(std::to_string(simTime().dbl()));
        HTTPRequest httpr = HTTPRequest(maPortV1, maHostV1);
        std::string response = httpr.Request(reportStr);
    }
    if (sendReportsV2) {
        std::string reportStr = "curTime:";
        reportStr.append(std::to_string(simTime().dbl()));
        HTTPRequest httpr = HTTPRequest(maPortV2, maHostV2);
        std::string response = httpr.Request(reportStr);
    }

    if (writeSelfMsg) {
        writeSelfBsm(myBsm[0]);
    }

    if (writeListSelfMsg) {
        writeSelfListBsm(myBsm[0]);
    }

    if (myReportType == reportTypes::ProtocolReport) {
        handleReportProtocol(false);
    }

    if (EnablePC) {
        pcPolicy.checkPseudonymChange(myPcType);
    }

//this method is for self messages (mostly timers)
//it is important to call the BaseWaveApplLayer function for BSM and WSM transmission

}

void JosephVeinsApp::handleReportProtocol(bool lastTimeStep) {

    unsigned long pseudosList[MAX_REP_PSEUDOS];
    if (EnableV1) {
        int pseudoNum = 0;
        if (lastTimeStep) {
            pseudoNum = reportProtocolEnforcerV1.getAllReportPseudoes(
                    simTime().dbl(), pseudosList);
        } else {
            pseudoNum = reportProtocolEnforcerV1.getReportPseudoes(
                    simTime().dbl(), pseudosList);
        }

        for (int var = 0; var < pseudoNum; ++var) {
            if (!detectedNodes.includes(pseudosList[var])) {
                BasicSafetyMessage * reportedBsm =
                        detectedNodes.getNodeHistoryAddr(pseudosList[var])->getLatestBSMAddr();

                BsmCheck * reportedCheck = detectedNodes.getMDMHistoryAddr(
                        pseudosList[var])->getLatestBsmCheckAddr(1);

                MDReport reportBase;
                reportBase.setGenerationTime(simTime().dbl());
                reportBase.setSenderPseudo(myPseudonym);
                reportBase.setReportedPseudo(pseudosList[var]);

                reportBase.setSenderRealId(myId);
                reportBase.setReportedRealId(reportedBsm->getSenderRealId());

                reportBase.setMbType(
                        mbTypes::mbNames[reportedBsm->getSenderMbType()]);
                reportBase.setAttackType(
                        attackTypes::AttackNames[reportedBsm->getSenderAttackType()]);
                std::pair<double, double> currLonLat = traci->getLonLat(
                        curPosition);
                reportBase.setSenderGps(
                        Coord(currLonLat.first, currLonLat.second));
                reportBase.setReportedGps(
                        reportedBsm->getSenderGpsCoordinates());

                ProtocolReport hir = ProtocolReport(reportBase);
                hir.addEvidence(myBsm[0], *reportedCheck, *reportedBsm,
                        &detectedNodes, simTime().dbl(),
                        CollectionPeriod, 1);

                if (writeReportsV1 && myBsmNum > 0) {
                    hir.writeStrToFile(savePath, serialNumber, "V1",
                            hir.getReportPrintableJson(), curDate);
                }

                if (writeListReportsV1 && myBsmNum > 0) {
                    hir.writeStrToFileList(savePath, serialNumber, "V1",
                            hir.getReportPrintableJson(), curDate);
                }

                if (sendReportsV1 && myBsmNum > 0) {
                    HTTPRequest httpr = HTTPRequest(maPortV1, maHostV1);
                    std::string response = httpr.Request(
                            hir.getReportPrintableJson());
                }
            } else {
                reportProtocolEnforcerV1.removeReportedPseudo(pseudosList[var]);
            }
        }

    }

    if (EnableV2) {

        int pseudoNum = 0;
        if (lastTimeStep) {
            pseudoNum = reportProtocolEnforcerV2.getAllReportPseudoes(
                    simTime().dbl(), pseudosList);
        } else {
            pseudoNum = reportProtocolEnforcerV2.getReportPseudoes(
                    simTime().dbl(), pseudosList);
        }

        for (int var = 0; var < pseudoNum; ++var) {
            if (detectedNodes.includes(pseudosList[var])) {
                BasicSafetyMessage * reportedBsm =
                        detectedNodes.getNodeHistoryAddr(pseudosList[var])->getLatestBSMAddr();
                BsmCheck * reportedCheck = detectedNodes.getMDMHistoryAddr(
                        pseudosList[var])->getLatestBsmCheckAddr(2);
                if (reportedBsm->getSenderPseudonym() == 0) {
                    std::cout << pseudosList[var] << "\n";
                    std::cout << "Problem Found \n";
                    exit(0);
                }

                MDReport reportBase;
                reportBase.setGenerationTime(simTime().dbl());
                reportBase.setSenderPseudo(myPseudonym);
                reportBase.setReportedPseudo(pseudosList[var]);

                reportBase.setSenderRealId(myId);
                reportBase.setReportedRealId(reportedBsm->getSenderRealId());

                reportBase.setMbType(
                        mbTypes::mbNames[reportedBsm->getSenderMbType()]);
                reportBase.setAttackType(
                        attackTypes::AttackNames[reportedBsm->getSenderAttackType()]);
                std::pair<double, double> currLonLat = traci->getLonLat(
                        curPosition);
                reportBase.setSenderGps(
                        Coord(currLonLat.first, currLonLat.second));
                reportBase.setReportedGps(
                        reportedBsm->getSenderGpsCoordinates());

                ProtocolReport hir = ProtocolReport(reportBase);

                hir.addEvidence(myBsm[0], *reportedCheck, *reportedBsm,
                        &detectedNodes, simTime().dbl(),
                        CollectionPeriod, 2);

                if (writeReportsV2 && myBsmNum > 0) {
                    hir.writeStrToFile(savePath, serialNumber, "V2",
                            hir.getReportPrintableJson(), curDate);
                }

                if (writeListReportsV2 && myBsmNum > 0) {
                    hir.writeStrToFileList(savePath, serialNumber, "V2",
                            hir.getReportPrintableJson(), curDate);
                }

                if (sendReportsV2 && myBsmNum > 0) {
                    HTTPRequest httpr = HTTPRequest(maPortV2, maHostV2);
                    std::string response = httpr.Request(
                            hir.getReportPrintableJson());
                }
            } else {
                reportProtocolEnforcerV2.removeReportedPseudo(pseudosList[var]);
            }
        }

    }

}
void JosephVeinsApp::handlePositionUpdate(cObject* obj) {
    BaseWaveApplLayer::handlePositionUpdate(obj);

    ChannelMobilityPtrType const mobility = check_and_cast<
            ChannelMobilityPtrType>(obj);

    RelativeOffsetConf relativeOffsetConfidence = RelativeOffsetConf(
            &ConfPosMax, &ConfSpeedMax, &ConfHeadMax, &ConfAccelMax,
            &deltaConfPos, &deltaConfSpeed, &deltaConfHead, &deltaConfAccel);

    if (veremiConf) {
        double posEtx = genLib.GaussianRandomDouble(
                (curPositionConfidence.x + curPositionConfidenceOrig.x) / 2,
                0.03 * curPositionConfidenceOrig.x);
        double posEty = genLib.GaussianRandomDouble(
                (curPositionConfidence.y + curPositionConfidenceOrig.y) / 2,
                0.03 * curPositionConfidenceOrig.y);

        curPositionConfidence = Coord(posEtx, posEty, 0);

        curHeadingConfidence = Coord(
                curHeadingConfidenceOrig.x * exp(-0.1 *  mobility->getCurrentSpeed().x),
                curHeadingConfidenceOrig.y * exp(-0.1 *  mobility->getCurrentSpeed().y), 0);

        Coord oldSpeedConfidence = Coord(curSpeedConfidence.x,
                curSpeedConfidence.y, curSpeedConfidence.z);
        curSpeedConfidence = Coord(curSpeed.x * curSpeedConfidenceOrig.x,
                curSpeed.y * curSpeedConfidenceOrig.y, 0);

        curAccelConfidence = Coord(
                fabs((curSpeedConfidence.x - oldSpeedConfidence.x))
                        / (simTime().dbl() - lastPositionUpdate),
                        fabs((curSpeedConfidence.y - oldSpeedConfidence.y))
                        / (simTime().dbl() - lastPositionUpdate),
                        fabs((curSpeedConfidence.z - oldSpeedConfidence.z))
                        / (simTime().dbl() - lastPositionUpdate));

        lastPositionUpdate = simTime().dbl();

        //curHeadingConfidence = Coord(0, 0, 0);

        RelativeOffset relativeOffset = RelativeOffset(&curPositionConfidence,
                &curSpeedConfidence, &curHeadingConfidence, &curAccelConfidence,
                &deltaRPosition, &deltaThetaPosition, &deltaSpeed,
                &deltaHeading, &deltaAccel);

        curPosition = relativeOffset.OffsetPosition(
                mobility->getPositionAt(simTime()));
        curSpeed = relativeOffset.OffsetSpeed(mobility->getCurrentSpeed());
        curHeading = relativeOffset.OffsetHeading(
                mobility->getCurrentDirection());
        curAccel = relativeOffset.OffsetAccel(mobility->getCurrentAccel());


    } else {
        curPositionConfidence = relativeOffsetConfidence.OffsetPosConf(
                curPositionConfidenceOrig);
        curSpeedConfidence = relativeOffsetConfidence.OffsetSpeedConf(
                curSpeedConfidenceOrig);
        curHeadingConfidence = relativeOffsetConfidence.OffsetHeadingConf(
                curHeadingConfidenceOrig);
        curAccelConfidence = relativeOffsetConfidence.OffsetAccelConf(
                curAccelConfidenceOrig);

        RelativeOffset relativeOffset = RelativeOffset(&curPositionConfidence,
                &curSpeedConfidence, &curHeadingConfidence, &curAccelConfidence,
                &deltaRPosition, &deltaThetaPosition, &deltaSpeed,
                &deltaHeading, &deltaAccel);

        //    GaussianRandom relativeOffset = GaussianRandom(&curPositionConfidence,
        //            &curSpeedConfidence, &curHeadingConfidence);

        curPosition = relativeOffset.OffsetPosition(
                mobility->getPositionAt(simTime()));
        curSpeed = relativeOffset.OffsetSpeed(mobility->getCurrentSpeed());
        curHeading = relativeOffset.OffsetHeading(
                mobility->getCurrentDirection());
        curAccel = relativeOffset.OffsetAccel(mobility->getCurrentAccel());
    }

    if (writeVeReMi) {
        VeReMi.serializeRawData(&curPosition, &curPositionConfidence, &curSpeed,
                &curSpeedConfidence, &curHeading, &curHeadingConfidence,
                &curAccel, &curAccelConfidence);
    }

//the vehicle has moved. Code that reacts to new positions goes here.
//member variables such as currentPosition and currentSpeed are updated in the parent class
}

// code is based on TracingApp::populateWSM(WaveShortMessage* wsm, int rcvId, int serial)
void JosephVeinsApp::populateWSM(BaseFrame1609_4* wsm, LAddress::L2Type rcvId,
        int serial) {
    BaseWaveApplLayer::populateWSM(wsm, rcvId, serial);
    if (BasicSafetyMessage* bsm = dynamic_cast<BasicSafetyMessage*>(wsm)) {

        //F2MD
        bsm->setSenderPseudonym(myPseudonym);
        bsm->setSenderMbType(myMdType);
        bsm->setSenderAttackType(myAttackType);
        // Genuine
        bsm->setSenderPos(curPosition);
        bsm->setSenderPosConfidence(curPositionConfidence);

        std::pair<double, double> currLonLat = traci->getLonLat(curPosition);
        bsm->setSenderGpsCoordinates(
                Coord(currLonLat.first, currLonLat.second));

        bsm->setSenderSpeed(curSpeed);
        bsm->setSenderSpeedConfidence(curSpeedConfidence);

        bsm->setSenderHeading(curHeading);
        bsm->setSenderHeadingConfidence(curHeadingConfidence);

        bsm->setSenderAccel(curAccel);
        bsm->setSenderAccelConfidence(curAccelConfidence);

        bsm->setSenderWidth(myWidth);
        bsm->setSenderLength(myLength);

        bsm->setSenderRealId(myId);
        addMyBsm(*bsm);
        // Genuine

        if (myMdType == mbTypes::LocalAttacker) {
            if (attackBsm.getSenderPseudonym() != 0) {
                bsm->setSenderPseudonym(attackBsm.getSenderPseudonym());

                bsm->setSenderPos(attackBsm.getSenderPos());
                bsm->setSenderPosConfidence(attackBsm.getSenderPosConfidence());

                std::pair<double, double> currLonLat = traci->getLonLat(
                        attackBsm.getSenderPos());
                bsm->setSenderGpsCoordinates(
                        Coord(currLonLat.first, currLonLat.second));

                bsm->setSenderSpeed(attackBsm.getSenderSpeed());
                bsm->setSenderSpeedConfidence(
                        attackBsm.getSenderSpeedConfidence());

                bsm->setSenderHeading(attackBsm.getSenderHeading());
                bsm->setSenderHeadingConfidence(
                        attackBsm.getSenderHeadingConfidence());

                bsm->setSenderAccel(attackBsm.getSenderAccel());
                bsm->setSenderAccelConfidence(
                        attackBsm.getSenderAccelConfidence());

                bsm->setSenderWidth(attackBsm.getSenderWidth());
                bsm->setSenderLength(attackBsm.getSenderLength());
            } else {
                bsm->setSenderPos(curPosition);
                bsm->setSenderPosConfidence(curPositionConfidence);

                std::pair<double, double> currLonLat = traci->getLonLat(
                        curPosition);
                bsm->setSenderGpsCoordinates(
                        Coord(currLonLat.first, currLonLat.second));

                bsm->setSenderSpeed(curSpeed);
                bsm->setSenderSpeedConfidence(curSpeedConfidence);

                bsm->setSenderHeading(curHeading);
                bsm->setSenderHeadingConfidence(curHeadingConfidence);

                bsm->setSenderAccel(curAccel);
                bsm->setSenderAccelConfidence(curAccelConfidence);

                bsm->setSenderWidth(myWidth);
                bsm->setSenderLength(myLength);

                bsm->setSenderMbType(mbTypes::Genuine);
                //bsm->setSenderAttackType(attackTypes::Genuine);
            }
        }

//        GeneralLib genLib = GeneralLib();
//        Coord posConf = bsm->getSenderPosConfidence();
//        posConf = Coord(posConf.x + genLib.RandomDouble(0, 0.1*posConf.x) ,posConf.y + genLib.RandomDouble(0, 0.1*posConf.y),posConf.z);
//        bsm->setSenderPosConfidence(posConf);
//
//        Coord spdConf = bsm->getSenderSpeedConfidence();
//        spdConf = Coord(spdConf.x + genLib.RandomDouble(0, 0.1*spdConf.x) ,spdConf.y + genLib.RandomDouble(0, 0.1*spdConf.y),spdConf.z);
//        bsm->setSenderSpeedConfidence(spdConf);
//
//        Coord accConf = bsm->getSenderAccelConfidence();
//        accConf = Coord(accConf.x + genLib.RandomDouble(0, 0.1*accConf.x) ,accConf.y + genLib.RandomDouble(0, 0.1*accConf.y),accConf.z);
//        bsm->setSenderAccelConfidence(accConf);
//
//        Coord headConf = bsm->getSenderHeadingConfidence();
//        headConf = Coord(headConf.x + genLib.RandomDouble(0, 0.1*headConf.x) ,headConf.y + genLib.RandomDouble(0, 0.1*headConf.y),headConf.z);
//        bsm->setSenderHeadingConfidence(headConf);

        if (writeVeReMi) {
            VeReMi.serializeGroundTruth(bsm);
        }
    }
}

void JosephVeinsApp::addTargetNode(unsigned long id) {
    bool found = false;
    for (int var = 0; var < targetNodesLength; ++var) {
        if (targetNodes[var] == id) {
            found = true;
        }
    }

    if (!found) {
        targetNodes[targetNodesLength] = id;
        targetNodesLength++;
    }
}
void JosephVeinsApp::removeTargetNode(unsigned long id) {
    int index = -1;
    for (int var = 0; var < targetNodesLength; ++var) {
        if (targetNodes[var] == id) {
            index = var;
            break;
        }
    }

    for (int var = index; var < targetNodesLength - 1; ++var) {
        targetNodes[var] = targetNodes[var + 1];
    }
    targetNodesLength--;
}
void JosephVeinsApp::clearTargetNodes() {
    targetNodesLength = 0;
}
bool JosephVeinsApp::isTargetNode(unsigned long id) {
    for (int var = 0; var < targetNodesLength; ++var) {
        if (targetNodes[var] == id) {
            return true;
        }
    }
    return false;
}

void JosephVeinsApp::addAccusedNode(unsigned long id) {
    bool found = false;
    for (int var = 0; var < accusedNodesLength; ++var) {
        if (accusedNodes[var] == id) {
            found = true;
        }
    }

    if (!found) {
        accusedNodes[accusedNodesLength] = id;
        accusedNodesLength++;
    }
}
void JosephVeinsApp::removeAccusedNode(unsigned long id) {
    int index = -1;
    for (int var = 0; var < accusedNodesLength; ++var) {
        if (accusedNodes[var] == id) {
            index = var;
            break;
        }
    }

    for (int var = index; var < accusedNodesLength - 1; ++var) {
        accusedNodes[var] = accusedNodes[var + 1];
    }
    accusedNodesLength--;
}

void JosephVeinsApp::clearAccusedNodes() {
    accusedNodesLength = 0;
}

bool JosephVeinsApp::isAccusedNode(unsigned long id) {
    for (int var = 0; var < accusedNodesLength; ++var) {
        if (accusedNodes[var] == id) {
            return true;
        }
    }
    return false;
}

//F2MD
void JosephVeinsApp::addMyBsm(BasicSafetyMessage bsm) {
    if (myBsmNum < MYBSM_SIZE) {
        myBsmNum++;
    }
    for (int var = myBsmNum - 1; var > 0; --var) {
        myBsm[var] = myBsm[var - 1];
    }

    myBsm[0] = bsm;
}
