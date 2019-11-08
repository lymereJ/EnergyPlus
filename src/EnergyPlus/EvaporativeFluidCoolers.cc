// EnergyPlus, Copyright (c) 1996-2019, The Board of Trustees of the University of Illinois,
// The Regents of the University of California, through Lawrence Berkeley National Laboratory
// (subject to receipt of any required approvals from the U.S. Dept. of Energy), Oak Ridge
// National Laboratory, managed by UT-Battelle, Alliance for Sustainable Energy, LLC, and other
// contributors. All rights reserved.
//
// NOTICE: This Software was developed under funding from the U.S. Department of Energy and the
// U.S. Government consequently retains certain rights. As such, the U.S. Government has been
// granted for itself and others acting on its behalf a paid-up, nonexclusive, irrevocable,
// worldwide license in the Software to reproduce, distribute copies to the public, prepare
// derivative works, and perform publicly and display publicly, and to permit others to do so.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright notice, this list of
//     conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//
// (3) Neither the name of the University of California, Lawrence Berkeley National Laboratory,
//     the University of Illinois, U.S. Dept. of Energy nor the names of its contributors may be
//     used to endorse or promote products derived from this software without specific prior
//     written permission.
//
// (4) Use of EnergyPlus(TM) Name. If Licensee (i) distributes the software in stand-alone form
//     without changes from the version obtained under this License, or (ii) Licensee makes a
//     reference solely to the software portion of its product, Licensee must refer to the
//     software as "EnergyPlus version X" software, where "X" is the version number Licensee
//     obtained under this License and may not use a different name for the software. Except as
//     specifically required in this Section (4), Licensee shall not use in a company name, a
//     product name, in advertising, publicity, or other promotional activities any name, trade
//     name, trademark, logo, or other designation of "EnergyPlus", "E+", "e+" or confusingly
//     similar designation, without the U.S. Department of Energy's prior written consent.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// C++ Headers
#include <cassert>
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/Array.functions.hh>
#include <ObjexxFCL/Fmath.hh>
#include <ObjexxFCL/gio.hh>
#include <ObjexxFCL/string.functions.hh>

// EnergyPlus Headers
#include <EnergyPlus/BranchNodeConnections.hh>
#include <EnergyPlus/DataBranchAirLoopPlant.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataIPShortCuts.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataPlant.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/DataWater.hh>
#include <EnergyPlus/EvaporativeFluidCoolers.hh>
#include <EnergyPlus/FluidProperties.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/GlobalNames.hh>
#include <EnergyPlus/InputProcessing/InputProcessor.hh>
#include <EnergyPlus/NodeInputManager.hh>
#include <EnergyPlus/OutAirNodeManager.hh>
#include <EnergyPlus/OutputProcessor.hh>
#include <EnergyPlus/OutputReportPredefined.hh>
#include <EnergyPlus/PlantUtilities.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ReportSizingManager.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>
#include <EnergyPlus/WaterManager.hh>

namespace EnergyPlus {

namespace EvaporativeFluidCoolers {

    // Module containing the routines dealing with the objects EvaporativeFluidCooler:SingleSpeed and
    // EvaporativeFluidCooler:TwoSpeed

    // MODULE INFORMATION:
    //       AUTHOR         Chandan Sharma
    //       DATE WRITTEN   May 2009
    //       MODIFIED       na
    //       RE-ENGINEERED  na

    // PURPOSE OF THIS MODULE:
    // Model the performance of evaporative fluid coolers

    // METHODOLOGY EMPLOYED:
    // Based on cooling tower by Shirey, Raustad: Dec 2000; Shirey, Sept 2002
    
    std::string const cEvapFluidCooler_SingleSpeed("EvaporativeFluidCooler:SingleSpeed");
    std::string const cEvapFluidCooler_TwoSpeed("EvaporativeFluidCooler:TwoSpeed");

    bool GetEvapFluidCoolerInputFlag(true);
    int NumSimpleEvapFluidCoolers(0); // Number of similar evaporative fluid coolers

    Array1D<EvapFluidCoolerSpecs> SimpleEvapFluidCooler; // dimension to number of machines
    std::unordered_map<std::string, std::string> UniqueSimpleEvapFluidCoolerNames;

    void SimEvapFluidCoolers(std::string const &EvapFluidCoolerType,
                             std::string const &EvapFluidCoolerName,
                             int &CompIndex,
                             bool &RunFlag,
                             bool const InitLoopEquip,
                             Real64 &MaxCap,
                             Real64 &MinCap,
                             Real64 &OptCap,
                             bool const GetSizingFactor, // TRUE when just the sizing factor is requested
                             Real64 &SizingFactor        // sizing factor
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Main evaporative fluid cooler driver subroutine.  Gets called from
        // PlantCondLoopSupplySideManager.

        // METHODOLOGY EMPLOYED:
        // After being called by PlantCondLoopSupplySideManager, this subroutine
        // calls GetEvapFluidCoolerInput to get all evaporative fluid cooler input info (one time only),
        // then calls the appropriate subroutine to calculate evaporative fluid cooler performance,
        // update records (node info) and writes output report info.

        // REFERENCES:
        // Based on SimTowers subroutine by Fred Buhl, May 2002

        int EvapFluidCoolerNum; // Pointer to EvapFluidCooler

        if (GetEvapFluidCoolerInputFlag) {
            GetEvapFluidCoolerInput();
            GetEvapFluidCoolerInputFlag = false;
        }

        // Find the correct EvapCooler
        if (CompIndex == 0) {
            EvapFluidCoolerNum = UtilityRoutines::FindItemInList(EvapFluidCoolerName, SimpleEvapFluidCooler);
            if (EvapFluidCoolerNum == 0) {
                ShowFatalError("SimEvapFluidCoolers: Unit not found = " + EvapFluidCoolerName);
            }
            CompIndex = EvapFluidCoolerNum;
        } else {
            EvapFluidCoolerNum = CompIndex;
            if (EvapFluidCoolerNum > NumSimpleEvapFluidCoolers || EvapFluidCoolerNum < 1) {
                ShowFatalError("SimEvapFluidCoolers:  Invalid CompIndex passed = " + General::TrimSigDigits(EvapFluidCoolerNum) +
                               ", Number of Units = " + General::TrimSigDigits(NumSimpleEvapFluidCoolers) + ", Entered Unit name = " + EvapFluidCoolerName);
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).CheckEquipName) {
                if (EvapFluidCoolerName != SimpleEvapFluidCooler(EvapFluidCoolerNum).Name) {
                    ShowFatalError("SimEvapFluidCoolers: Invalid CompIndex passed = " + General::TrimSigDigits(EvapFluidCoolerNum) + ", Unit name = " +
                                   EvapFluidCoolerName + ", stored Unit Name for that index = " + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
                }
                SimpleEvapFluidCooler(EvapFluidCoolerNum).CheckEquipName = false;
            }
        }

        SimpleEvapFluidCooler(EvapFluidCoolerNum).AirFlowRateRatio = 0.0; // Ratio of air flow rate through VS Evaporative fluid cooler to design air flow rate

        {
            auto SELECT_CASE_var(SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerType_Num);

            if (SELECT_CASE_var == EvapFluidCooler::SingleSpeed) {

                if (InitLoopEquip) {
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).InitEvapFluidCooler();
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).SizeEvapFluidCooler(EvapFluidCoolerNum);
                    MinCap = 0.0; // signifies non-load based model (i.e. forward
                    MaxCap = SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity *
                             SimpleEvapFluidCooler(EvapFluidCoolerNum).HeatRejectCapNomCapSizingRatio;
                    OptCap = SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity;
                    if (GetSizingFactor) {
                        SizingFactor = SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac;
                    }
                    return;
                }
                SimpleEvapFluidCooler(EvapFluidCoolerNum).InitEvapFluidCooler();
                SimpleEvapFluidCooler(EvapFluidCoolerNum).CalcSingleSpeedEvapFluidCooler(EvapFluidCoolerNum);
                SimpleEvapFluidCooler(EvapFluidCoolerNum).CalculateWaterUseage();
                SimpleEvapFluidCooler(EvapFluidCoolerNum).UpdateEvapFluidCooler();
                SimpleEvapFluidCooler(EvapFluidCoolerNum).ReportEvapFluidCooler(RunFlag);

            } else if (SELECT_CASE_var == EvapFluidCooler::TwoSpeed) {
                if (GetSizingFactor) {
                    SizingFactor = SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac;
                    return;
                }
                if (InitLoopEquip) {
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).InitEvapFluidCooler();
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).SizeEvapFluidCooler(EvapFluidCoolerNum);
                    MinCap = 0.0; // signifies non-load based model (i.e. forward
                    MaxCap = 0.0; // heat exhanger model)
                    OptCap = 0.0;
                    return;
                }
                SimpleEvapFluidCooler(EvapFluidCoolerNum).InitEvapFluidCooler();
                SimpleEvapFluidCooler(EvapFluidCoolerNum).CalcTwoSpeedEvapFluidCooler(EvapFluidCoolerNum);
                SimpleEvapFluidCooler(EvapFluidCoolerNum).CalculateWaterUseage();
                SimpleEvapFluidCooler(EvapFluidCoolerNum).UpdateEvapFluidCooler();
                SimpleEvapFluidCooler(EvapFluidCoolerNum).ReportEvapFluidCooler(RunFlag);

            } else {
                ShowFatalError("SimEvapFluidCoolers: Invalid evaporative fluid cooler Type Requested = " + EvapFluidCoolerType);
            }
        } // TypeOfEquip
    }

    void GetEvapFluidCoolerInput()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Chandan Sharma
        //       DATE WRITTEN:    May 2009
        //       MODIFIED         Chandan Sharma, April 2010
        //       RE-ENGINEERED    na

        // PURPOSE OF THIS SUBROUTINE:
        // Obtains input data for evaporative fluid coolers and stores it in SimpleEvapFluidCooler data structure.

        // METHODOLOGY EMPLOYED:
        // Uses "Get" routines to read in the data.

        // REFERENCES:
        // Based on GetTowerInput subroutine from Don Shirey, Jan 2001 and Sept/Oct 2002
        // B.A. Qureshi and S.M. Zubair , Prediction of evaporation losses in evaporative fluid coolers
        // Applied thermal engineering 27 (2007) 520-527
        
        int NumAlphas;                        // Number of elements in the alpha array
        int NumNums;                          // Number of elements in the numeric array
        int IOStat;                           // IO Status when calling get input subroutine
        bool ErrorsFound(false);       // Logical flag set .TRUE. if errors found while getting input data
        Array1D<Real64> NumArray(25);         // Numeric input data array
        Array1D_string AlphArray(13);         // Character string input data array
        std::string FluidName;

        // Get number of all evaporative fluid coolers specified in the input data file (idf)
        int NumSingleSpeedEvapFluidCoolers = inputProcessor->getNumObjectsFound(cEvapFluidCooler_SingleSpeed);
        int NumTwoSpeedEvapFluidCoolers = inputProcessor->getNumObjectsFound(cEvapFluidCooler_TwoSpeed);
        NumSimpleEvapFluidCoolers = NumSingleSpeedEvapFluidCoolers + NumTwoSpeedEvapFluidCoolers;

        if (NumSimpleEvapFluidCoolers <= 0)
            ShowFatalError("No evaporative fluid cooler objects found in input, however, a branch object has specified an evaporative fluid cooler. "
                           "Search the input for evaporative fluid cooler to determine the cause for this error.");

        // See if load distribution manager has already gotten the input
        if (allocated(SimpleEvapFluidCooler)) return;
        GetEvapFluidCoolerInputFlag = false;

        // Allocate data structures to hold evaporative fluid cooler input data,
        // report data and evaporative fluid cooler inlet conditions
        SimpleEvapFluidCooler.allocate(NumSimpleEvapFluidCoolers);
        UniqueSimpleEvapFluidCoolerNames.reserve(static_cast<unsigned>(NumSimpleEvapFluidCoolers));

        // Load data structures with evaporative fluid cooler input data
        DataIPShortCuts::cCurrentModuleObject = cEvapFluidCooler_SingleSpeed;
        for (int SingleSpeedEvapFluidCoolerNumber = 1; SingleSpeedEvapFluidCoolerNumber <= NumSingleSpeedEvapFluidCoolers;
             ++SingleSpeedEvapFluidCoolerNumber) {
            int EvapFluidCoolerNum = SingleSpeedEvapFluidCoolerNumber;
            inputProcessor->getObjectItem(DataIPShortCuts::cCurrentModuleObject,
                                          SingleSpeedEvapFluidCoolerNumber,
                                          AlphArray,
                                          NumAlphas,
                                          NumArray,
                                          NumNums,
                                          IOStat,
                                          _,
                                          DataIPShortCuts::lAlphaFieldBlanks,
                                          DataIPShortCuts::cAlphaFieldNames,
                                          DataIPShortCuts::cNumericFieldNames);
            GlobalNames::VerifyUniqueInterObjectName(
                UniqueSimpleEvapFluidCoolerNames, AlphArray(1), DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaFieldNames(1), ErrorsFound);

            SimpleEvapFluidCooler(EvapFluidCoolerNum).Name = AlphArray(1);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerType = DataIPShortCuts::cCurrentModuleObject;
            SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerType_Num = EvapFluidCooler::SingleSpeed;
            SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerMassFlowRateMultiplier = 2.5;
            SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterInletNodeNum = NodeInputManager::GetOnlySingleNode(
                AlphArray(2), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, AlphArray(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Inlet, 1, DataLoopNode::ObjectIsNotParent);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterOutletNodeNum = NodeInputManager::GetOnlySingleNode(
                AlphArray(3), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, AlphArray(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 1, DataLoopNode::ObjectIsNotParent);
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject, AlphArray(1), AlphArray(2), AlphArray(3), "Chilled Water Nodes");
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate = NumArray(1);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRateWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower = NumArray(2);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPowerWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignSprayWaterFlowRate = NumArray(3);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HeatRejectCapNomCapSizingRatio = NumArray(4);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity = NumArray(5);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA = NumArray(6);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUAWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate = NumArray(7);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRateWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedUserSpecifiedDesignCapacity = NumArray(8);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringWaterTemp = NumArray(9);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirTemp = NumArray(10);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp = NumArray(11);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).FluidIndex = DataPlant::PlantLoop(DataSizing::CurLoopNum).FluidIndex;
            FluidName = FluidProperties::GetGlycolNameByIndex(SimpleEvapFluidCooler(EvapFluidCoolerNum).FluidIndex);

            if (DataIPShortCuts::lAlphaFieldBlanks(4) || AlphArray(4).empty()) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + ", \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\" Performance input method is not specified. ");
                ErrorsFound = true;
            }
            if (UtilityRoutines::SameString(AlphArray(4), "STANDARDDESIGNCAPACITY")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::StandardDesignCapacity;
                if (FluidName != "WATER") {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    R"(\". StandardDesignCapacity performance input method is only valid for fluid type = "Water".)");
                    ShowContinueError("Currently, Fluid Type = " + FluidName + " in CondenserLoop = " + DataPlant::PlantLoop(DataSizing::CurLoopNum).Name);
                    ErrorsFound = true;
                }
            }

            // outdoor air inlet node
            if (DataIPShortCuts::lAlphaFieldBlanks(5)) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).OutdoorAirInletNodeNum = 0;
            } else {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).OutdoorAirInletNodeNum = NodeInputManager::GetOnlySingleNode(AlphArray(5),
                                                                                                     ErrorsFound,
                                                                                                     DataIPShortCuts::cCurrentModuleObject,
                                                                                                     SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                                                                                     DataLoopNode::NodeType_Air,
                                                                                                     DataLoopNode::NodeConnectionType_OutsideAirReference,
                                                                                                     1,
                                                                                                     DataLoopNode::ObjectIsNotParent);
                if (!OutAirNodeManager::CheckOutAirNodeNumber(SimpleEvapFluidCooler(EvapFluidCoolerNum).OutdoorAirInletNodeNum)) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + ", \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    "\" Outdoor Air Inlet DataLoopNode::Node Name not valid Outdoor Air DataLoopNode::Node= " + AlphArray(5));
                    ShowContinueError("...does not appear in an OutdoorAir:NodeList or as an OutdoorAir:DataLoopNode::Node.");
                    ErrorsFound = true;
                }
            }

            //   fluid bypass for single speed evaporative fluid cooler
            if (DataIPShortCuts::lAlphaFieldBlanks(6) || AlphArray(6).empty()) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).CapacityControl = 0; // FanCycling
            } else {
                {
                    auto const SELECT_CASE_var(UtilityRoutines::MakeUPPERCase(AlphArray(6)));
                    if (SELECT_CASE_var == "FANCYCLING") {
                        SimpleEvapFluidCooler(EvapFluidCoolerNum).CapacityControl = 0;
                    } else if (SELECT_CASE_var == "FLUIDBYPASS") {
                        SimpleEvapFluidCooler(EvapFluidCoolerNum).CapacityControl = 1;
                    } else {
                        SimpleEvapFluidCooler(EvapFluidCoolerNum).CapacityControl = 0;
                        ShowWarningError(DataIPShortCuts::cCurrentModuleObject + ", \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                         "\" The Capacity Control is not specified correctly. The default Fan Cycling is used.");
                    }
                }
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac = NumArray(12); //  N11  \field Sizing Factor
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac <= 0.0) SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac = 1.0;

            // begin water use and systems get input
            if (UtilityRoutines::SameString(AlphArray(7), "LossFactor")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapLossMode = EvapLoss::ByUserFactor;
            } else if (UtilityRoutines::SameString(AlphArray(7), "SaturatedExit")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapLossMode = EvapLoss::ByMoistTheory;
            } else if (AlphArray(7).empty()) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapLossMode = EvapLoss::ByMoistTheory;
            } else {
                ShowSevereError("Invalid, " + DataIPShortCuts::cAlphaFieldNames(7) + " = " + AlphArray(7));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + " = " + AlphArray(1));
                ErrorsFound = true;
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor = NumArray(13); //  N13 , \field Evaporation Loss Factor
            if ((NumNums < 13) && (SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor == 0.0)) {
                // assume Evaporation loss factor not entered and should be calculated
                if ((DataEnvironment::OutRelHumValue >= 0.1) && (DataEnvironment::OutRelHumValue <= 0.7)) {
                    // Use correlation by B.A. Qureshi and S.M. Zubair if within these limits
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor =
                        (113.0 - 8.417 * DataEnvironment::OutRelHumValue + 1.6147 * DataEnvironment::OutDryBulbTemp) * 1.0e-5;
                } else { // Inlet conditions are out of the limit of correlation; An approximate default value of loss factor is used
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor = 0.2;
                }
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftLossFraction = NumArray(14) / 100.0; //  N14, \field Drift Loss Percent

            if ((NumNums < 13) && (SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftLossFraction == 0.0)) {
                // assume Drift loss not entered and should be defaulted
                SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftLossFraction = 0.008 / 100.0;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).ConcentrationRatio = NumArray(15); //  N15, \field Blowdown Concentration Ratio

            if (UtilityRoutines::SameString(AlphArray(8), "ScheduledRate")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode = Blowdown::BySchedule;
            } else if (UtilityRoutines::SameString(AlphArray(8), "ConcentrationRatio")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode = Blowdown::ByConcentration;
            } else if (AlphArray(8).empty()) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode = Blowdown::ByConcentration;
                if ((NumNums < 15) && (SimpleEvapFluidCooler(EvapFluidCoolerNum).ConcentrationRatio == 0.0)) {
                    // assume Concetration ratio was omitted and should be defaulted
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).ConcentrationRatio = 3.0;
                }
            } else {
                ShowSevereError("Invalid, " + DataIPShortCuts::cAlphaFieldNames(8) + " = " + AlphArray(8));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + " =" + AlphArray(1));
                ErrorsFound = true;
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).SchedIDBlowdown = ScheduleManager::GetScheduleIndex(AlphArray(9));
            if ((SimpleEvapFluidCooler(EvapFluidCoolerNum).SchedIDBlowdown == 0) &&
                (SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode == Blowdown::BySchedule)) {
                ShowSevereError("Invalid, " + DataIPShortCuts::cAlphaFieldNames(9) + " = " + AlphArray(9));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + " =" + AlphArray(1));
                ErrorsFound = true;
            }

            if (AlphArray(10).empty()) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).SuppliedByWaterSystem = false;
            } else { // water from storage tank
                WaterManager::SetupTankDemandComponent(AlphArray(1),
                                         DataIPShortCuts::cCurrentModuleObject,
                                         AlphArray(10),
                                         ErrorsFound,
                                         SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterTankID,
                                         SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterTankDemandARRID);
                SimpleEvapFluidCooler(EvapFluidCoolerNum).SuppliedByWaterSystem = true;
            }

            //   Check various inputs to ensure that all the required variables are specified.

            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignSprayWaterFlowRate <= 0.0) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler input requires a design spray water flow rate greater than zero for all performance "
                                "input methods.");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate <= 0.0 &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(1) +
                                "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower <= 0.0 &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(2) +
                                "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(4), "UFACTORTIMESAREAANDDESIGNWATERFLOWRATE")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::UFactor;
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(6) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(7) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
            } else if (UtilityRoutines::SameString(AlphArray(4), "STANDARDDESIGNCAPACITY")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::StandardDesignCapacity;
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(5) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
            } else if (UtilityRoutines::SameString(AlphArray(4), "USERSPECIFIEDDESIGNCAPACITY")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::UserSpecifiedDesignCapacity;
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(7) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedUserSpecifiedDesignCapacity <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(8) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringWaterTemp <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(9) +
                                    "\", entered value <= 0.0, but must be >0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirTemp <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(10) +
                                    "\", entered value <= 0.0, but must be >0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(11) +
                                    "\", entered value <= 0.0, but must be >0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringWaterTemp <=
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", " + DataIPShortCuts::cNumericFieldNames(9) + " must be greater than " +
                                    DataIPShortCuts::cNumericFieldNames(11) + '.');
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirTemp <=
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", " + DataIPShortCuts::cNumericFieldNames(10) + " must be greater than " +
                                    DataIPShortCuts::cNumericFieldNames(11) + '.');
                    ErrorsFound = true;
                }
            } else { // Evaporative fluid cooler performance input method is not specified as a valid "choice"
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler Performance Input Method must be \"UFactorTimesAreaAndDesignWaterFlowRate\" or "
                                "\"StandardDesignCapacity\" or \"UserSpecifiedDesignCapacity\".");
                ShowContinueError("Evaporative fluid cooler Performance Input Method currently specified as: " + AlphArray(4));
                ErrorsFound = true;
            }
        } // End Single-Speed Evaporative Fluid Cooler Loop

        DataIPShortCuts::cCurrentModuleObject = cEvapFluidCooler_TwoSpeed;
        for (int TwoSpeedEvapFluidCoolerNumber = 1; TwoSpeedEvapFluidCoolerNumber <= NumTwoSpeedEvapFluidCoolers; ++TwoSpeedEvapFluidCoolerNumber) {
            int EvapFluidCoolerNum = NumSingleSpeedEvapFluidCoolers + TwoSpeedEvapFluidCoolerNumber;
            inputProcessor->getObjectItem(DataIPShortCuts::cCurrentModuleObject,
                                          TwoSpeedEvapFluidCoolerNumber,
                                          AlphArray,
                                          NumAlphas,
                                          NumArray,
                                          NumNums,
                                          IOStat,
                                          _,
                                          DataIPShortCuts::lAlphaFieldBlanks,
                                          DataIPShortCuts::cAlphaFieldNames,
                                          DataIPShortCuts::cNumericFieldNames);

            GlobalNames::VerifyUniqueInterObjectName(
                UniqueSimpleEvapFluidCoolerNames, AlphArray(1), DataIPShortCuts::cCurrentModuleObject, DataIPShortCuts::cAlphaFieldNames(1), ErrorsFound);

            SimpleEvapFluidCooler(EvapFluidCoolerNum).Name = AlphArray(1);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerType = DataIPShortCuts::cCurrentModuleObject;
            SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerType_Num = EvapFluidCooler::TwoSpeed;
            SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapFluidCoolerMassFlowRateMultiplier = 2.5;
            SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterInletNodeNum = NodeInputManager::GetOnlySingleNode(
                AlphArray(2), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, AlphArray(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Inlet, 1, DataLoopNode::ObjectIsNotParent);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterOutletNodeNum = NodeInputManager::GetOnlySingleNode(
                AlphArray(3), ErrorsFound, DataIPShortCuts::cCurrentModuleObject, AlphArray(1), DataLoopNode::NodeType_Water, DataLoopNode::NodeConnectionType_Outlet, 1, DataLoopNode::ObjectIsNotParent);
            BranchNodeConnections::TestCompSet(DataIPShortCuts::cCurrentModuleObject, AlphArray(1), AlphArray(2), AlphArray(3), "Chilled Water Nodes");

            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate = NumArray(1);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRateWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower = NumArray(2);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPowerWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRate = NumArray(3);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRate == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRateWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRateSizingFactor = NumArray(4);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPower = NumArray(5);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPower == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPowerWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPowerSizingFactor = NumArray(6);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignSprayWaterFlowRate = NumArray(7);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HeatRejectCapNomCapSizingRatio = NumArray(8);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity = NumArray(9);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedStandardDesignCapacity = NumArray(10);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedStandardDesignCapacitySizingFactor = NumArray(11);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA = NumArray(12);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUAWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA = NumArray(13);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUAWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUASizingFactor = NumArray(14);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate = NumArray(15);
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate == DataSizing::AutoSize) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRateWasAutoSized = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedUserSpecifiedDesignCapacity = NumArray(16);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedUserSpecifiedDesignCapacity = NumArray(17);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedUserSpecifiedDesignCapacitySizingFactor = NumArray(18);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringWaterTemp = NumArray(19);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirTemp = NumArray(20);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp = NumArray(21);
            SimpleEvapFluidCooler(EvapFluidCoolerNum).FluidIndex = DataPlant::PlantLoop(DataSizing::CurLoopNum).FluidIndex;
            FluidName = FluidProperties::GetGlycolNameByIndex(SimpleEvapFluidCooler(EvapFluidCoolerNum).FluidIndex);

            if (DataIPShortCuts::lAlphaFieldBlanks(4)) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + ", \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\" Performance input method is not specified. ");
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(4), "STANDARDDESIGNCAPACITY")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::StandardDesignCapacity;
                if (FluidName != "WATER") {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    R"(". StandardDesignCapacity performance input method is only valid for fluid type = "Water".)");
                    ShowContinueError("Currently, Fluid Type = " + FluidName + " in CondenserLoop = " + DataPlant::PlantLoop(DataSizing::CurLoopNum).Name);
                    ErrorsFound = true;
                }
            }

            // outdoor air inlet node
            if (DataIPShortCuts::lAlphaFieldBlanks(5)) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).OutdoorAirInletNodeNum = 0;
            } else {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).OutdoorAirInletNodeNum = NodeInputManager::GetOnlySingleNode(AlphArray(5),
                                                                                                     ErrorsFound,
                                                                                                     DataIPShortCuts::cCurrentModuleObject,
                                                                                                     SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                                                                                     DataLoopNode::NodeType_Air,
                                                                                                     DataLoopNode::NodeConnectionType_OutsideAirReference,
                                                                                                     1,
                                                                                                     DataLoopNode::ObjectIsNotParent);
                if (!OutAirNodeManager::CheckOutAirNodeNumber(SimpleEvapFluidCooler(EvapFluidCoolerNum).OutdoorAirInletNodeNum)) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + ", \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    "\" Outdoor Air Inlet DataLoopNode::Node Name not valid Outdoor Air DataLoopNode::Node= " + AlphArray(5));
                    ShowContinueError("...does not appear in an OutdoorAir:NodeList or as an OutdoorAir:DataLoopNode::Node.");
                    ErrorsFound = true;
                }
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac = NumArray(22); //  N16  \field Sizing Factor
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac <= 0.0) SimpleEvapFluidCooler(EvapFluidCoolerNum).SizFac = 1.0;

            // begin water use and systems get input
            if (UtilityRoutines::SameString(AlphArray(6), "LossFactor")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapLossMode = EvapLoss::ByUserFactor;
            } else if (UtilityRoutines::SameString(AlphArray(6), "SaturatedExit")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapLossMode = EvapLoss::ByMoistTheory;
            } else if (DataIPShortCuts::lAlphaFieldBlanks(6)) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvapLossMode = EvapLoss::ByMoistTheory;
            } else {
                ShowSevereError("Invalid " + DataIPShortCuts::cAlphaFieldNames(6) + " = " + AlphArray(6));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + " = " + AlphArray(1));
                ErrorsFound = true;
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor = NumArray(23); //  N23 , \field Evaporation Loss Factor
            if ((NumNums < 23) && (SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor == 0.0)) {
                // assume Evaporation loss factor not entered and should be calculated
                if ((DataEnvironment::OutRelHumValue >= 0.1) && (DataEnvironment::OutRelHumValue <= 0.7)) {
                    // Use correlation by B.A. Qureshi and S.M. Zubair if within these limits
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor =
                        (113.0 - 8.417 * DataEnvironment::OutRelHumValue + 1.6147 * DataEnvironment::OutDryBulbTemp) * 1.0e-5;
                } else { // Inlet conditions are out of the limit of correlation; An approximate default value of loss factor is used
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).UserEvapLossFactor = 0.2;
                }
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftLossFraction = NumArray(24) / 100.0; //  N24, \field Drift Loss Percent
            if ((NumNums < 24) && (SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftLossFraction == 0.0)) {
                // assume Drift loss not entered and should be defaulted
                SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftLossFraction = 0.008 / 100.0;
            }

            SimpleEvapFluidCooler(EvapFluidCoolerNum).ConcentrationRatio = NumArray(25); //  N25, \field Blowdown Concentration Ratio

            if (UtilityRoutines::SameString(AlphArray(7), "ScheduledRate")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode = Blowdown::BySchedule;
            } else if (UtilityRoutines::SameString(AlphArray(7), "ConcentrationRatio")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode = Blowdown::ByConcentration;
            } else if (DataIPShortCuts::lAlphaFieldBlanks(7)) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode = Blowdown::ByConcentration;
                if ((NumNums < 25) && (SimpleEvapFluidCooler(EvapFluidCoolerNum).ConcentrationRatio == 0.0)) {
                    // assume Concetration ratio was omitted and should be defaulted
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).ConcentrationRatio = 3.0;
                }
            } else {
                ShowSevereError("Invalid " + DataIPShortCuts::cAlphaFieldNames(7) + " = " + AlphArray(7));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + " = " + AlphArray(1));
                ErrorsFound = true;
            }
            SimpleEvapFluidCooler(EvapFluidCoolerNum).SchedIDBlowdown = ScheduleManager::GetScheduleIndex(AlphArray(8));
            if ((SimpleEvapFluidCooler(EvapFluidCoolerNum).SchedIDBlowdown == 0) &&
                (SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownMode == Blowdown::BySchedule)) {
                ShowSevereError("Invalid " + DataIPShortCuts::cAlphaFieldNames(8) + " = " + AlphArray(8));
                ShowContinueError("Entered in " + DataIPShortCuts::cCurrentModuleObject + " = " + AlphArray(1));
                ErrorsFound = true;
            }

            if (DataIPShortCuts::lAlphaFieldBlanks(9)) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).SuppliedByWaterSystem = false;
            } else { // water from storage tank
                WaterManager::SetupTankDemandComponent(AlphArray(1),
                                         DataIPShortCuts::cCurrentModuleObject,
                                         AlphArray(9),
                                         ErrorsFound,
                                         SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterTankID,
                                         SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterTankDemandARRID);
                SimpleEvapFluidCooler(EvapFluidCoolerNum).SuppliedByWaterSystem = true;
            }

            //   Check various inputs to ensure that all the required variables are specified.

            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignSprayWaterFlowRate <= 0.0) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler input requires a design spray water flow rate greater than zero for all performance "
                                "input methods.");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate <= 0.0 &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + "= \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler input requires design air flow rate at high fan speed to be greater than zero for all "
                                "performance input methods.");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRate <= 0.0 &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRate != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + "= \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler input requires design air flow rate at low fan speed to be greater than zero for all "
                                "performance input methods.");
                ErrorsFound = true;
            }
            //   High speed air flow rate must be greater than low speed air flow rate.
            //   Can't tell yet if autosized, check later in InitEvapFluidCooler.
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate <= SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedAirFlowRate &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedAirFlowRate != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler air flow rate at low fan speed must be less than the air flow rate at high fan speed.");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower <= 0.0 &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(2) +
                                "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPower <= 0.0 &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPower != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(5) +
                                "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                ErrorsFound = true;
            }
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower <= SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedFanPower &&
                SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedFanPower != DataSizing::AutoSize) {
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler low speed fan power must be less than the high speed fan power .");
                ErrorsFound = true;
            }

            if (UtilityRoutines::SameString(AlphArray(4), "UFACTORTIMESAREAANDDESIGNWATERFLOWRATE")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::UFactor;
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(12) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(13) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA <=
                        SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    "\". Evaporative fluid cooler U-factor Times Area Value at Low Fan Speed must be less than the U-factor Times "
                                    "Area Value at High Fan Speed.");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(15) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
            } else if (UtilityRoutines::SameString(AlphArray(4), "STANDARDDESIGNCAPACITY")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::StandardDesignCapacity;
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(9) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedStandardDesignCapacity <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(10) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedStandardDesignCapacity >=
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedStandardDesignCapacity) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    "\". Low-Speed Standard Design Capacity must be less than the High-Speed Standard Design Capacity.");
                    ErrorsFound = true;
                }
            } else if (UtilityRoutines::SameString(AlphArray(4), "USERSPECIFIEDDESIGNCAPACITY")) {
                SimpleEvapFluidCooler(EvapFluidCoolerNum).PerformanceInputMethod_Num = PIM::UserSpecifiedDesignCapacity;
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate <= 0.0 &&
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignWaterFlowRate != DataSizing::AutoSize) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(15) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedUserSpecifiedDesignCapacity <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(16) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedUserSpecifiedDesignCapacity <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(17) +
                                    "\", entered value <= 0.0, but must be > 0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA != 0.0) {
                    if (SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedEvapFluidCoolerUA > 0.0) {
                        ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                        "\". UserSpecifiedDesignCapacity performance input method and evaporative fluid cooler UA at high fan speed "
                                        "have been specified.");
                    } else {
                        ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                        "\". UserSpecifiedDesignCapacity performance input method has been specified and evaporative fluid cooler UA "
                                        "at high fan speed is being autosized.");
                    }
                    ShowContinueError("Evaporative fluid cooler UA at high fan speed must be left blank when UserSpecifiedDesignCapacity performance "
                                      "input method is used.");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA != 0.0) {
                    if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedEvapFluidCoolerUA > 0.0) {
                        ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                        "\". UserSpecifiedDesignCapacity performance input method and evaporative fluid cooler UA at low fan speed "
                                        "have been specified.");
                    } else {
                        ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                        "\". UserSpecifiedDesignCapacity performance input method has been specified and evaporative fluid cooler UA "
                                        "at low fan speed is being autosized.");
                    }
                    ShowContinueError("Evaporative fluid cooler UA at low fan speed must be left blank when UserSpecifiedDesignCapacity performance "
                                      "input method is used.");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).LowSpeedUserSpecifiedDesignCapacity >=
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).HighSpeedUserSpecifiedDesignCapacity) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                    "\". Low-Speed User Specified Design Capacity must be less than the High-Speed User Specified Design Dapacity.");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringWaterTemp <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(19) +
                                    "\", entered value <= 0.0, but must be >0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirTemp <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(20) +
                                    "\", entered value <= 0.0, buy must be >0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp <= 0.0) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", invalid data for \"" + DataIPShortCuts::cNumericFieldNames(21) +
                                    "\", entered value <= 0.0, but must be >0 for " + DataIPShortCuts::cAlphaFieldNames(4) + " = \"" + AlphArray(4) + "\".");
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringWaterTemp <=
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", " + DataIPShortCuts::cNumericFieldNames(19) + " must be greater than " +
                                    DataIPShortCuts::cNumericFieldNames(15) + '.');
                    ErrorsFound = true;
                }
                if (SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirTemp <=
                    SimpleEvapFluidCooler(EvapFluidCoolerNum).DesignEnteringAirWetBulbTemp) {
                    ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + AlphArray(1) + "\", " + DataIPShortCuts::cNumericFieldNames(20) + " must be greater than " +
                                    DataIPShortCuts::cNumericFieldNames(15) + '.');
                    ErrorsFound = true;
                }
            } else { // Evaporative fluid cooler performance input method is not specified as a valid "choice"
                ShowSevereError(DataIPShortCuts::cCurrentModuleObject + " = \"" + SimpleEvapFluidCooler(EvapFluidCoolerNum).Name +
                                "\". Evaporative fluid cooler Performance Input Method must be \"UFactorTimesAreaAndDesignWaterFlowRate\" or "
                                "\"StandardDesignCapacity\" or \"UserSpecifiedDesignCapacity\".");
                ShowContinueError("Evaporative fluid cooler Performanace Input Method currently specified as: " + AlphArray(4));
                ErrorsFound = true;
            }
        } // End Two-Speed Evaporative Fluid Cooler Loop

        if (ErrorsFound) {
            ShowFatalError("Errors found in getting evaporative fluid cooler input.");
        }

        // Set up output variables
        // CurrentModuleObject='EvaporativeFluidCooler:SingleSpeed'
        for (int EvapFluidCoolerNum = 1; EvapFluidCoolerNum <= NumSingleSpeedEvapFluidCoolers; ++EvapFluidCoolerNum) {
            SetupOutputVariable("Cooling Tower Inlet Temperature",
                                OutputProcessor::Unit::C,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).fluidCoolerInletWaterTemp,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Outlet Temperature",
                                OutputProcessor::Unit::C,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).fluidCoolerOutletWaterTemp,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterMassFlowRate,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Qactual,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Fan Electric Power",
                                OutputProcessor::Unit::W,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).FanPower,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Fan Electric Energy",
                                OutputProcessor::Unit::J,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).FanEnergy,
                                "System",
                                "Sum",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                _,
                                "Electric",
                                "HeatRejection",
                                _,
                                "Plant");
            // Added for fluid bypass
            SetupOutputVariable("Cooling Tower Bypass Fraction",
                                OutputProcessor::Unit::None,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).BypassFraction,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
        }

        // CurrentModuleObject='EvaporativeFluidCooler:TwoSpeed'
        for (int EvapFluidCoolerNum = NumSingleSpeedEvapFluidCoolers + 1;
             EvapFluidCoolerNum <= NumSingleSpeedEvapFluidCoolers + NumTwoSpeedEvapFluidCoolers;
             ++EvapFluidCoolerNum) {
            SetupOutputVariable("Cooling Tower Inlet Temperature",
                                OutputProcessor::Unit::C,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).fluidCoolerInletWaterTemp,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Outlet Temperature",
                                OutputProcessor::Unit::C,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).fluidCoolerOutletWaterTemp,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Mass Flow Rate",
                                OutputProcessor::Unit::kg_s,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterMassFlowRate,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Heat Transfer Rate",
                                OutputProcessor::Unit::W,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Qactual,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Fan Electric Power",
                                OutputProcessor::Unit::W,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).FanPower,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Fan Electric Energy",
                                OutputProcessor::Unit::J,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).FanEnergy,
                                "System",
                                "Sum",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                _,
                                "Electric",
                                "HeatRejection",
                                _,
                                "Plant");
        }

        // setup common water reporting for all types of evaporative fluid coolers.
        // CurrentModuleObject='EvaporativeFluidCooler:*'
        for (int EvapFluidCoolerNum = 1; EvapFluidCoolerNum <= NumSingleSpeedEvapFluidCoolers + NumTwoSpeedEvapFluidCoolers; ++EvapFluidCoolerNum) {
            if (SimpleEvapFluidCooler(EvapFluidCoolerNum).SuppliedByWaterSystem) {
                SetupOutputVariable("Cooling Tower Make Up Water Volume Flow Rate",
                                    OutputProcessor::Unit::m3_s,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).MakeUpVdot,
                                    "System",
                                    "Average",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
                SetupOutputVariable("Cooling Tower Make Up Water Volume",
                                    OutputProcessor::Unit::m3,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).MakeUpVol,
                                    "System",
                                    "Sum",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
                SetupOutputVariable("Cooling Tower Storage Tank Water Volume Flow Rate",
                                    OutputProcessor::Unit::m3_s,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).TankSupplyVdot,
                                    "System",
                                    "Average",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
                SetupOutputVariable("Cooling Tower Storage Tank Water Volume",
                                    OutputProcessor::Unit::m3,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).TankSupplyVol,
                                    "System",
                                    "Sum",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                    _,
                                    "Water",
                                    "HeatRejection",
                                    _,
                                    "Plant");
                SetupOutputVariable("Cooling Tower Starved Storage Tank Water Volume Flow Rate",
                                    OutputProcessor::Unit::m3_s,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).StarvedMakeUpVdot,
                                    "System",
                                    "Average",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
                SetupOutputVariable("Cooling Tower Starved Storage Tank Water Volume",
                                    OutputProcessor::Unit::m3,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).StarvedMakeUpVol,
                                    "System",
                                    "Sum",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                    _,
                                    "Water",
                                    "HeatRejection",
                                    _,
                                    "Plant");
                SetupOutputVariable("Cooling Tower Make Up Mains Water Volume",
                                    OutputProcessor::Unit::m3,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).StarvedMakeUpVol,
                                    "System",
                                    "Sum",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                    _,
                                    "MainsWater",
                                    "HeatRejection",
                                    _,
                                    "Plant");
            } else { // Evaporative fluid cooler water from mains and gets metered
                SetupOutputVariable("Cooling Tower Make Up Water Volume Flow Rate",
                                    OutputProcessor::Unit::m3_s,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).MakeUpVdot,
                                    "System",
                                    "Average",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
                SetupOutputVariable("Cooling Tower Make Up Water Volume",
                                    OutputProcessor::Unit::m3,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).MakeUpVol,
                                    "System",
                                    "Sum",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                    _,
                                    "Water",
                                    "HeatRejection",
                                    _,
                                    "Plant");
                SetupOutputVariable("Cooling Tower Make Up Mains Water Volume",
                                    OutputProcessor::Unit::m3,
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).MakeUpVol,
                                    "System",
                                    "Sum",
                                    SimpleEvapFluidCooler(EvapFluidCoolerNum).Name,
                                    _,
                                    "MainsWater",
                                    "HeatRejection",
                                    _,
                                    "Plant");
            }

            SetupOutputVariable("Cooling Tower Water Evaporation Volume Flow Rate",
                                OutputProcessor::Unit::m3_s,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvaporationVdot,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Water Evaporation Volume",
                                OutputProcessor::Unit::m3,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).EvaporationVol,
                                "System",
                                "Sum",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Water Drift Volume Flow Rate",
                                OutputProcessor::Unit::m3_s,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftVdot,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Water Drift Volume",
                                OutputProcessor::Unit::m3,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).DriftVol,
                                "System",
                                "Sum",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Water Blowdown Volume Flow Rate",
                                OutputProcessor::Unit::m3_s,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownVdot,
                                "System",
                                "Average",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
            SetupOutputVariable("Cooling Tower Water Blowdown Volume",
                                OutputProcessor::Unit::m3,
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).BlowdownVol,
                                "System",
                                "Sum",
                                SimpleEvapFluidCooler(EvapFluidCoolerNum).Name);
        } // loop all evaporative fluid coolers
    }

    void EvapFluidCoolerSpecs::InitEvapFluidCooler()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for initializations of the evaporative fluid cooler components and for
        // final checking of evaporative fluid cooler inputs (post autosizing)

        // METHODOLOGY EMPLOYED:
        // Uses the status flags to trigger initializations.

        // REFERENCES:
        // Based on InitTower subroutine by Don Shirey Sept/Oct 2002, F Buhl Oct 2002

        std::string const RoutineName("InitEvapFluidCooler");

        bool ErrorsFound(false); // Flag if input data errors are found
        if (this->MyOneTimeFlag) {
            this->MyOneTimeFlag = false;
        }

        if (this->OneTimeFlagForEachEvapFluidCooler) {
            int TypeOf_Num = 0;
            if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                TypeOf_Num = DataPlant::TypeOf_EvapFluidCooler_SingleSpd;
            } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                TypeOf_Num = DataPlant::TypeOf_EvapFluidCooler_TwoSpd;
            } else {
                assert(false);
            }
            ErrorsFound = false;
            // Locate the tower on the plant loops for later usage
            PlantUtilities::ScanPlantLoopsForObject(this->Name,
                                    TypeOf_Num,
                                    this->LoopNum,
                                    this->LoopSideNum,
                                    this->BranchNum,
                                    this->CompNum,
                                    ErrorsFound,
                                    _,
                                    _,
                                    _,
                                    _,
                                    _);

            if (ErrorsFound) {
                ShowFatalError("InitEvapFluidCooler: Program terminated due to previous condition(s).");
            }

            if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                if (this->DesignWaterFlowRate > 0.0) {
                    if (this->HighSpeedAirFlowRate <=
                        this->LowSpeedAirFlowRate) {
                        ShowSevereError("EvaporativeFluidCooler:TwoSpeed \"" + this->Name +
                                        "\". Low speed air flow rate must be less than the high speed air flow rate.");
                        ErrorsFound = true;
                    }
                    if ((this->HighSpeedEvapFluidCoolerUA > 0.0) &&
                        (this->LowSpeedEvapFluidCoolerUA > 0.0) &&
                        (this->HighSpeedEvapFluidCoolerUA <=
                         this->LowSpeedEvapFluidCoolerUA)) {
                        ShowSevereError(
                            "EvaporativeFluidCooler:TwoSpeed \"" + this->Name +
                            "\". Evaporative fluid cooler UA at low fan speed must be less than the evaporative fluid cooler UA at high fan speed.");
                        ErrorsFound = true;
                    }
                }
            }

            if (ErrorsFound) {
                ShowFatalError("InitEvapFluidCooler: Program terminated due to previous condition(s).");
            }

            this->OneTimeFlagForEachEvapFluidCooler = false;
        }

        // Begin environment initializations
        if (this->MyEnvrnFlag && DataGlobals::BeginEnvrnFlag && (DataPlant::PlantFirstSizesOkayToFinalize)) {

            Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                   DataGlobals::InitConvTemp,
                                   DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                   RoutineName);
            this->DesWaterMassFlowRate = this->DesignWaterFlowRate * rho;
            PlantUtilities::InitComponentNodes(0.0,
                               this->DesWaterMassFlowRate,
                               this->WaterInletNodeNum,
                               this->WaterOutletNodeNum,
                               this->LoopNum,
                               this->LoopSideNum,
                               this->BranchNum,
                               this->CompNum);
            this->MyEnvrnFlag = false;
        }

        if (!DataGlobals::BeginEnvrnFlag) {
            this->MyEnvrnFlag = true;
        }

        // Each time initializations
        this->WaterInletNode = this->WaterInletNodeNum;
        this->inletConds.WaterTemp = DataLoopNode::Node(this->WaterInletNode).Temp;

        if (this->OutdoorAirInletNodeNum != 0) {
            this->inletConds.AirTemp = DataLoopNode::Node(this->OutdoorAirInletNodeNum).Temp;
            this->inletConds.AirHumRat = DataLoopNode::Node(this->OutdoorAirInletNodeNum).HumRat;
            this->inletConds.AirPress = DataLoopNode::Node(this->OutdoorAirInletNodeNum).Press;
            this->inletConds.AirWetBulb =
                DataLoopNode::Node(this->OutdoorAirInletNodeNum).OutAirWetBulb;
        } else {
            this->inletConds.AirTemp = DataEnvironment::OutDryBulbTemp;
            this->inletConds.AirHumRat = DataEnvironment::OutHumRat;
            this->inletConds.AirPress = DataEnvironment::OutBaroPress;
            this->inletConds.AirWetBulb = DataEnvironment::OutWetBulbTemp;
        }

        this->WaterMassFlowRate = PlantUtilities::RegulateCondenserCompFlowReqOp(this->LoopNum,
                                                           this->LoopSideNum,
                                                           this->BranchNum,
                                                           this->CompNum,
                                                           this->DesWaterMassFlowRate *
                                                               this->EvapFluidCoolerMassFlowRateMultiplier);

        PlantUtilities::SetComponentFlowRate(this->WaterMassFlowRate,
                             this->WaterInletNodeNum,
                             this->WaterOutletNodeNum,
                             this->LoopNum,
                             this->LoopSideNum,
                             this->BranchNum,
                             this->CompNum);
    }

    void EvapFluidCoolerSpecs::SizeEvapFluidCooler(int const EvapFluidCoolerNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       Chandan Sharma, April 2010
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for sizing evaporative fluid cooler Components for which capacities and flow rates
        // have not been specified in the input. This subroutine also calculates evaporative fluid cooler UA if the user
        // has specified evaporative fluid cooler performance via the "Standard Design Capacity" method.

        // METHODOLOGY EMPLOYED:
        // Obtains condenser flow rate from the plant sizing array. If evaporative fluid cooler performance is specified
        // via the "Standard Design Capacity" method, the water flow rate is directly proportional to capacity.

        // REFERENCES:
        // Based on SizeTower by Don Shirey, Sept/Oct 2002; Richard Raustad, Feb 2005

        int const MaxIte(500);    // Maximum number of iterations
        Real64 const Acc(0.0001); // Accuracy of result
        std::string const CalledFrom("SizeEvapFluidCooler");

        int SolFla;                      // Flag of solver
        Real64 UA;                       // Calculated UA value [W/C]
        Real64 OutWaterTempAtUA0;        // Water outlet temperature at UA0
        Real64 OutWaterTempAtUA1;        // Water outlet temperature at UA1
        Real64 DesignEnteringAirWetBulb; // Intermediate variable to check that design exit
        // temperature specified in the plant:sizing object
        // is higher than the design entering air wet-bulb temp
        // when autosize feature is used
        Array1D<Real64> Par(6); // Parameter array need for RegulaFalsi routine

        Real64 DesEvapFluidCoolerLoad = 0.0;   // Design evaporative fluid cooler load [W]
        Real64 tmpDesignWaterFlowRate = this->DesignWaterFlowRate;
        Real64 tmpHighSpeedFanPower = this->HighSpeedFanPower;
        Real64 tmpHighSpeedAirFlowRate = this->HighSpeedAirFlowRate;

        int PltSizCondNum = DataPlant::PlantLoop(this->LoopNum).PlantSizNum;

        if (this->DesignWaterFlowRateWasAutoSized &&
            this->PerformanceInputMethod_Num != PIM::StandardDesignCapacity) {
            if (PltSizCondNum > 0) {
                if (DataSizing::PlantSizData(PltSizCondNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                    tmpDesignWaterFlowRate = DataSizing::PlantSizData(PltSizCondNum).DesVolFlowRate * this->SizFac;
                    if (DataPlant::PlantFirstSizesOkayToFinalize) this->DesignWaterFlowRate = tmpDesignWaterFlowRate;

                } else {
                    tmpDesignWaterFlowRate = 0.0;
                    if (DataPlant::PlantFirstSizesOkayToFinalize) this->DesignWaterFlowRate = tmpDesignWaterFlowRate;
                }
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                           this->Name,
                                           "Design Water Flow Rate [m3/s]",
                                           this->DesignWaterFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                           this->Name,
                                           "Initial Design Water Flow Rate [m3/s]",
                                           this->DesignWaterFlowRate);
                    }
                }
            } else {
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("Autosizing error for evaporative fluid cooler object = " + this->Name);
                    ShowFatalError("Autosizing of evaporative fluid cooler condenser flow rate requires a loop Sizing:Plant object.");
                }
            }
            // Check when the user specified Condenser/Evaporative Fluid Cooler water design setpoint
            // temperature is less than design inlet air wet bulb temperature
            if (this->PerformanceInputMethod_Num == PIM::UFactor) {
                DesignEnteringAirWetBulb = 25.6;
            } else {
                DesignEnteringAirWetBulb = this->DesignEnteringAirWetBulbTemp;
            }
            if (DataSizing::PlantSizData(PltSizCondNum).ExitTemp <= DesignEnteringAirWetBulb) {
                ShowSevereError(
                    "Error when autosizing the UA value for Evaporative Fluid Cooler = " + this->Name + '.');
                ShowContinueError("Design Loop Exit Temperature (" + General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).ExitTemp, 2) +
                                  " C) must be greater than design entering air wet-bulb temperature (" +
                                  General::RoundSigDigits(DesignEnteringAirWetBulb, 2) + " C) when autosizing the Evaporative Fluid Cooler UA.");
                ShowContinueError("It is recommended that the Design Loop Exit Temperature = Design Entering Air Wet-bulb Temp plus the Evaporative "
                                  "Fluid Cooler design approach temperature (e.g., 4 C).");
                ShowContinueError("If using HVACTemplate:Plant:ChilledWaterLoop, then check that input field Condenser Water Design Setpoint must be "
                                  "> Design Entering Air Wet-bulb Temp if autosizing the Evaporative Fluid Cooler.");
                ShowFatalError("Review and revise design input values as appropriate.");
            }
        }

        if (this->PerformanceInputMethod_Num == PIM::UFactor &&
            !this->HighSpeedEvapFluidCoolerUAWasAutoSized) {
            if (PltSizCondNum > 0) {
                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                       DataGlobals::InitConvTemp,
                                       DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                       CalledFrom);
                Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                           DataSizing::PlantSizData(PltSizCondNum).ExitTemp,
                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                           CalledFrom);
                DesEvapFluidCoolerLoad = rho * Cp * tmpDesignWaterFlowRate * DataSizing::PlantSizData(PltSizCondNum).DeltaT;
                this->HighSpeedStandardDesignCapacity =
                    DesEvapFluidCoolerLoad / this->HeatRejectCapNomCapSizingRatio;
            } else {
                this->HighSpeedStandardDesignCapacity = 0.0;
            }
        }

        if (this->PerformanceInputMethod_Num == PIM::StandardDesignCapacity) {
            // Design water flow rate is assumed to be 3 gpm per ton (SI equivalent 5.382E-8 m3/s per watt)
            tmpDesignWaterFlowRate = 5.382e-8 * this->HighSpeedStandardDesignCapacity;
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                this->DesignWaterFlowRate = tmpDesignWaterFlowRate;
                if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Design Water Flow Rate based on evaporative fluid cooler Standard Design Capacity [m3/s]",
                                           this->DesignWaterFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Initial Design Water Flow Rate based on evaporative fluid cooler Standard Design Capacity [m3/s]",
                                           this->DesignWaterFlowRate);
                    }
                } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Design Water Flow Rate based on evaporative fluid cooler high-speed Standard Design Capacity [m3/s]",
                                           this->DesignWaterFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(
                            cEvapFluidCooler_TwoSpeed,
                            this->Name,
                            "Initial Design Water Flow Rate based on evaporative fluid cooler high-speed Standard Design Capacity [m3/s]",
                            this->DesignWaterFlowRate);
                    }
                }
            }
        }

        PlantUtilities::RegisterPlantCompDesignFlow(this->WaterInletNodeNum, tmpDesignWaterFlowRate);

        if (this->HighSpeedFanPowerWasAutoSized) {
            // We assume the nominal fan power is 0.0105 times the design load
            if (this->PerformanceInputMethod_Num == PIM::StandardDesignCapacity) {
                tmpHighSpeedFanPower = 0.0105 * this->HighSpeedStandardDesignCapacity;
                if (DataPlant::PlantFirstSizesOkayToFinalize) this->HighSpeedFanPower = tmpHighSpeedFanPower;
            } else if (this->PerformanceInputMethod_Num == PIM::UserSpecifiedDesignCapacity) {
                tmpHighSpeedFanPower = 0.0105 * this->HighSpeedUserSpecifiedDesignCapacity;
                if (DataPlant::PlantFirstSizesOkayToFinalize) this->HighSpeedFanPower = tmpHighSpeedFanPower;
            } else {
                if (DesEvapFluidCoolerLoad > 0) {
                    tmpHighSpeedFanPower = 0.0105 * DesEvapFluidCoolerLoad;
                    if (DataPlant::PlantFirstSizesOkayToFinalize) this->HighSpeedFanPower = tmpHighSpeedFanPower;
                } else if (PltSizCondNum > 0) {
                    if (DataSizing::PlantSizData(PltSizCondNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                        Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                               DataGlobals::InitConvTemp,
                                               DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                               CalledFrom);
                        Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                                   DataSizing::PlantSizData(PltSizCondNum).ExitTemp,
                                                   DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                                   CalledFrom);
                        DesEvapFluidCoolerLoad = rho * Cp * tmpDesignWaterFlowRate * DataSizing::PlantSizData(PltSizCondNum).DeltaT;
                        tmpHighSpeedFanPower = 0.0105 * DesEvapFluidCoolerLoad;
                        if (DataPlant::PlantFirstSizesOkayToFinalize) this->HighSpeedFanPower = tmpHighSpeedFanPower;
                    } else {
                        tmpHighSpeedFanPower = 0.0;
                        if (DataPlant::PlantFirstSizesOkayToFinalize) this->HighSpeedFanPower = tmpHighSpeedFanPower;
                    }
                } else {
                    if (DataPlant::PlantFirstSizesOkayToFinalize) {
                        ShowSevereError("Autosizing of evaporative fluid cooler fan power requires a loop Sizing:Plant object.");
                        ShowFatalError(" Occurs in evaporative fluid cooler object= " + this->Name);
                    }
                }
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Fan Power at Design Air Flow Rate [W]",
                                           this->HighSpeedFanPower);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Initial Fan Power at Design Air Flow Rate [W]",
                                           this->HighSpeedFanPower);
                    }
                } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Fan Power at High Fan Speed [W]",
                                           this->HighSpeedFanPower);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Initial Fan Power at High Fan Speed [W]",
                                           this->HighSpeedFanPower);
                    }
                }
            }
        }

        if (this->HighSpeedAirFlowRateWasAutoSized) {
            // Plant Sizing Object is not required to AUTOSIZE this field since its simply a multiple of another field.

            tmpHighSpeedAirFlowRate = tmpHighSpeedFanPower * 0.5 * (101325.0 / DataEnvironment::StdBaroPress) / 190.0;
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                this->HighSpeedAirFlowRate = tmpHighSpeedAirFlowRate;

                if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Design Air Flow Rate [m3/s]",
                                           this->HighSpeedAirFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Initial Design Air Flow Rate [m3/s]",
                                           this->HighSpeedAirFlowRate);
                    }
                } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Air Flow Rate at High Fan Speed [m3/s]",
                                           this->HighSpeedAirFlowRate);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Initial Air Flow Rate at High Fan Speed [m3/s]",
                                           this->HighSpeedAirFlowRate);
                    }
                }
            }
        }

        if (this->HighSpeedEvapFluidCoolerUAWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize &&
            this->PerformanceInputMethod_Num == PIM::UFactor) {
            if (PltSizCondNum > 0) {
                if (DataSizing::PlantSizData(PltSizCondNum).DesVolFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                    // This conditional statement is to trap when the user specified Condenser/Evaporative Fluid Cooler water design setpoint
                    // temperature is less than design inlet air wet bulb temperature of 25.6 C
                    if (DataSizing::PlantSizData(PltSizCondNum).ExitTemp <= 25.6) {
                        ShowSevereError("Error when autosizing the UA value for Evaporative Fluid Cooler = " +
                                        this->Name + '.');
                        ShowContinueError("Design Loop Exit Temperature (" + General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).ExitTemp, 2) +
                                          " C) must be greater than 25.6 C when autosizing the Evaporative Fluid Cooler UA.");
                        ShowContinueError("The Design Loop Exit Temperature specified in Sizing:Plant object = " +
                                          DataSizing::PlantSizData(PltSizCondNum).PlantLoopName);
                        ShowContinueError("It is recommended that the Design Loop Exit Temperature = 25.6 C plus the Evaporative Fluid Cooler design "
                                          "approach temperature (e.g., 4 C).");
                        ShowContinueError("If using HVACTemplate:Plant:ChilledWaterLoop, then check that input field Condenser Water Design Setpoint "
                                          "must be > 25.6 C if autosizing the Evaporative Fluid Cooler.");
                        ShowFatalError("Review and revise design input values as appropriate.");
                    }
                    Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                           DataGlobals::InitConvTemp,
                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                           CalledFrom);
                    Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                               DataSizing::PlantSizData(PltSizCondNum).ExitTemp,
                                               DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                               CalledFrom);
                    DesEvapFluidCoolerLoad = rho * Cp * tmpDesignWaterFlowRate * DataSizing::PlantSizData(PltSizCondNum).DeltaT;
                    Par(1) = DesEvapFluidCoolerLoad;
                    Par(2) = double(EvapFluidCoolerNum);
                    Par(3) = rho * tmpDesignWaterFlowRate; // Design water mass flow rate
                    Par(4) = tmpHighSpeedAirFlowRate;      // Design air volume flow rate
                    Par(5) = Cp;

                    // Lower bound for UA [W/C]
                    Real64 UA0 = 0.0001 * DesEvapFluidCoolerLoad; // Assume deltaT = 10000K (limit)
                    Real64 UA1 = DesEvapFluidCoolerLoad;          // Assume deltaT = 1K
                    this->inletConds.WaterTemp =
                        DataSizing::PlantSizData(PltSizCondNum).ExitTemp + DataSizing::PlantSizData(PltSizCondNum).DeltaT;
                    this->inletConds.AirTemp = 35.0;
                    this->inletConds.AirWetBulb = 25.6;
                    this->inletConds.AirPress = DataEnvironment::StdBaroPress;
                    this->inletConds.AirHumRat =
                        Psychrometrics::PsyWFnTdbTwbPb(this->inletConds.AirTemp,
                                       this->inletConds.AirWetBulb,
                                       this->inletConds.AirPress);
                    General::SolveRoot(Acc, MaxIte, SolFla, UA, SimpleEvapFluidCoolerUAResidual, UA0, UA1, Par);
                    if (SolFla == -1) {
                        ShowWarningError("Iteration limit exceeded in calculating evaporative fluid cooler UA.");
                        ShowContinueError("Autosizing of fluid cooler UA failed for evaporative fluid cooler = " +
                                          this->Name);
                        ShowContinueError("The final UA value = " + General::RoundSigDigits(UA, 2) + "W/C, and the simulation continues...");
                    } else if (SolFla == -2) {
                        SimSimpleEvapFluidCooler(int(Par(2)), Par(3), Par(4), UA0, OutWaterTempAtUA0);
                        SimSimpleEvapFluidCooler(int(Par(2)), Par(3), Par(4), UA1, OutWaterTempAtUA1);
                        ShowSevereError(CalledFrom + ": The combination of design input values did not allow the calculation of a ");
                        ShowContinueError("reasonable UA value. Review and revise design input values as appropriate. Specifying hard");
                        ShowContinueError("sizes for some \"autosizable\" fields while autosizing other \"autosizable\" fields may be contributing "
                                          "to this problem.");
                        ShowContinueError("This model iterates on UA to find the heat transfer required to provide the design outlet ");
                        ShowContinueError("water temperature. Initially, the outlet water temperatures at high and low UA values are ");
                        ShowContinueError("calculated. The Design Exit Water Temperature should be between the outlet water ");
                        ShowContinueError("temperatures calculated at high and low UA values. If the Design Exit Water Temperature is ");
                        ShowContinueError("out of this range, the solution will not converge and UA will not be calculated. ");
                        ShowContinueError("The possible solutions could be to manually input adjusted water and/or air flow rates ");
                        ShowContinueError(
                            "based on the autosized values shown below or to adjust design evaporative fluid cooler air inlet wet-bulb temperature.");
                        ShowContinueError("Plant:Sizing object inputs also influence these results (e.g. DeltaT and ExitTemp).");
                        ShowContinueError("Inputs to the evaporative fluid cooler object:");
                        ShowContinueError("Design Evaporative Fluid Cooler Load [W]                      = " + General::RoundSigDigits(Par(1), 2));
                        ShowContinueError("Design Evaporative Fluid Cooler Water Volume Flow Rate [m3/s] = " +
                                          General::RoundSigDigits(this->DesignWaterFlowRate, 6));
                        ShowContinueError("Design Evaporative Fluid Cooler Air Volume Flow Rate [m3/s]   = " + General::RoundSigDigits(Par(4), 2));
                        ShowContinueError("Design Evaporative Fluid Cooler Air Inlet Wet-bulb Temp [C]   = " +
                                          General::RoundSigDigits(this->inletConds.AirWetBulb, 2));
                        ShowContinueError("Design Evaporative Fluid Cooler Water Inlet Temp [C]          = " +
                                          General::RoundSigDigits(this->inletConds.WaterTemp, 2));
                        ShowContinueError("Inputs to the plant sizing object:");
                        ShowContinueError("Design Exit Water Temp [C]                                    = " +
                                          General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).ExitTemp, 2));
                        ShowContinueError("Loop Design Temperature Difference [C]                        = " +
                                          General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).DeltaT, 2));
                        ShowContinueError("Design Evaporative Fluid Cooler Water Inlet Temp [C]          = " +
                                          General::RoundSigDigits(this->inletConds.WaterTemp, 2));
                        ShowContinueError("Calculated water outlet temperature at low UA [C](UA = " + General::RoundSigDigits(UA0, 2) +
                                          " W/C)  = " + General::RoundSigDigits(OutWaterTempAtUA0, 2));
                        ShowContinueError("Calculated water outlet temperature at high UA [C](UA = " + General::RoundSigDigits(UA1, 2) +
                                          " W/C)  = " + General::RoundSigDigits(OutWaterTempAtUA1, 2));
                        ShowFatalError("Autosizing of Evaporative Fluid Cooler UA failed for Evaporative Fluid Cooler = " +
                                       this->Name);
                    }
                    if (DataPlant::PlantFirstSizesOkayToFinalize)
                        this->HighSpeedEvapFluidCoolerUA = UA;
                    this->HighSpeedStandardDesignCapacity =
                        DesEvapFluidCoolerLoad / this->HeatRejectCapNomCapSizingRatio;
                } else {
                    if (DataPlant::PlantFirstSizesOkayToFinalize)
                        this->HighSpeedEvapFluidCoolerUA = 0.0;
                }
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                               this->Name,
                                               "U-Factor Times Area Value at Design Air Flow Rate [W/C]",
                                               this->HighSpeedEvapFluidCoolerUA);
                        }
                        if (DataPlant::PlantFirstSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                               this->Name,
                                               "Initial U-Factor Times Area Value at Design Air Flow Rate [W/C]",
                                               this->HighSpeedEvapFluidCoolerUA);
                        }
                    } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                        if (DataPlant::PlantFinalSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                               this->Name,
                                               "U-Factor Times Area Value at High Fan Speed [W/C]",
                                               this->HighSpeedEvapFluidCoolerUA);
                        }
                        if (DataPlant::PlantFirstSizesOkayToReport) {
                            ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                               this->Name,
                                               "Initial U-Factor Times Area Value at High Fan Speed [W/C]",
                                               this->HighSpeedEvapFluidCoolerUA);
                        }
                    }
                }
            } else {
                if (DataPlant::PlantFirstSizesOkayToFinalize) {
                    ShowSevereError("Autosizing error for evaporative fluid cooler object = " + this->Name);
                    ShowFatalError("Autosizing of evaporative fluid cooler UA requires a loop Sizing:Plant object.");
                }
            }
        }

        if (this->PerformanceInputMethod_Num == PIM::StandardDesignCapacity) {
            if (this->DesignWaterFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                // Standard Design Capacity doesn't include compressor heat;
                // predefined factor was 1.25 W heat rejection per W of delivered cooling, now a user input with 1.25 default
                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                       DataGlobals::InitConvTemp,
                                       DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                       CalledFrom);
                Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                           35.0,
                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                           CalledFrom);
                DesEvapFluidCoolerLoad = this->HighSpeedStandardDesignCapacity *
                                         this->HeatRejectCapNomCapSizingRatio;
                Par(1) = DesEvapFluidCoolerLoad;
                Par(2) = double(EvapFluidCoolerNum);
                Par(3) = rho * this->DesignWaterFlowRate; // Design water mass flow rate
                Par(4) = this->HighSpeedAirFlowRate;      // Design air volume flow rate
                Par(5) = Cp;
                Real64 UA0 = 0.0001 * DesEvapFluidCoolerLoad;                            // Assume deltaT = 10000K (limit)
                Real64 UA1 = DesEvapFluidCoolerLoad;                                     // Assume deltaT = 1K
                this->inletConds.WaterTemp = 35.0;  // 95F design inlet water temperature
                this->inletConds.AirTemp = 35.0;    // 95F design inlet air dry-bulb temp
                this->inletConds.AirWetBulb = 25.6; // 78F design inlet air wet-bulb temp
                this->inletConds.AirPress = DataEnvironment::StdBaroPress;
                this->inletConds.AirHumRat = Psychrometrics::PsyWFnTdbTwbPb(this->inletConds.AirTemp,
                                                                                          this->inletConds.AirWetBulb,
                                                                                          this->inletConds.AirPress);
                General::SolveRoot(Acc, MaxIte, SolFla, UA, SimpleEvapFluidCoolerUAResidual, UA0, UA1, Par);
                if (SolFla == -1) {
                    ShowWarningError("Iteration limit exceeded in calculating evaporative fluid cooler UA.");
                    ShowContinueError("Autosizing of fluid cooler UA failed for evaporative fluid cooler = " +
                                      this->Name);
                    ShowContinueError("The final UA value = " + General::RoundSigDigits(UA, 2) + "W/C, and the simulation continues...");
                } else if (SolFla == -2) {
                    ShowSevereError(CalledFrom + ": The combination of design input values did not allow the calculation of a ");
                    ShowContinueError("reasonable UA value. Review and revise design input values as appropriate. ");
                    ShowFatalError("Autosizing of Evaporative Fluid Cooler UA failed for Evaporative Fluid Cooler = " +
                                   this->Name);
                }
                this->HighSpeedEvapFluidCoolerUA = UA;
            } else {
                this->HighSpeedEvapFluidCoolerUA = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "U-Factor Times Area Value at Design Air Flow Rate [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Initial U-Factor Times Area Value at Design Air Flow Rate [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "U-Factor Times Area Value at High Fan Speed [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Initial U-Factor Times Area Value at High Fan Speed [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                }
            }
        }

        if (this->PerformanceInputMethod_Num == PIM::UserSpecifiedDesignCapacity) {
            if (this->DesignWaterFlowRate >= DataHVACGlobals::SmallWaterVolFlow) {
                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                       DataGlobals::InitConvTemp,
                                       DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                       CalledFrom);
                Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                           this->DesignEnteringWaterTemp,
                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                           CalledFrom);
                DesEvapFluidCoolerLoad = this->HighSpeedUserSpecifiedDesignCapacity;
                Par(1) = DesEvapFluidCoolerLoad;
                Par(2) = double(EvapFluidCoolerNum);
                Par(3) = rho * tmpDesignWaterFlowRate; // Design water mass flow rate
                Par(4) = tmpHighSpeedAirFlowRate;      // Design air volume flow rate
                Par(5) = Cp;
                Real64 UA0 = 0.0001 * DesEvapFluidCoolerLoad; // Assume deltaT = 10000K (limit)
                Real64 UA1 = DesEvapFluidCoolerLoad;          // Assume deltaT = 1K

                this->inletConds.WaterTemp = this->DesignEnteringWaterTemp;
                this->inletConds.AirTemp = this->DesignEnteringAirTemp;
                this->inletConds.AirWetBulb = this->DesignEnteringAirWetBulbTemp;
                this->inletConds.AirPress = DataEnvironment::StdBaroPress;
                this->inletConds.AirHumRat = Psychrometrics::PsyWFnTdbTwbPb(this->inletConds.AirTemp,
                                                                                          this->inletConds.AirWetBulb,
                                                                                          this->inletConds.AirPress);
                General::SolveRoot(Acc, MaxIte, SolFla, UA, SimpleEvapFluidCoolerUAResidual, UA0, UA1, Par);
                if (SolFla == -1) {
                    ShowWarningError("Iteration limit exceeded in calculating evaporative fluid cooler UA.");
                    ShowContinueError("Autosizing of fluid cooler UA failed for evaporative fluid cooler = " +
                                      this->Name);
                    ShowContinueError("The final UA value = " + General::RoundSigDigits(UA, 2) + "W/C, and the simulation continues...");
                } else if (SolFla == -2) {
                    SimSimpleEvapFluidCooler(int(Par(2)), Par(3), Par(4), UA0, OutWaterTempAtUA0);
                    SimSimpleEvapFluidCooler(int(Par(2)), Par(3), Par(4), UA1, OutWaterTempAtUA1);
                    ShowSevereError(CalledFrom + ": The combination of design input values did not allow the calculation of a ");
                    ShowContinueError("reasonable UA value. Review and revise design input values as appropriate. Specifying hard");
                    ShowContinueError(
                        R"(sizes for some "autosizable" fields while autosizing other "autosizable" fields may be contributing to this problem.)");
                    ShowContinueError("This model iterates on UA to find the heat transfer required to provide the design outlet ");
                    ShowContinueError("water temperature. Initially, the outlet water temperatures at high and low UA values are ");
                    ShowContinueError("calculated. The Design Exit Water Temperature should be between the outlet water ");
                    ShowContinueError("temperatures calculated at high and low UA values. If the Design Exit Water Temperature is ");
                    ShowContinueError("out of this range, the solution will not converge and UA will not be calculated. ");
                    ShowContinueError("The possible solutions could be to manually input adjusted water and/or air flow rates ");
                    ShowContinueError(
                        "based on the autosized values shown below or to adjust design evaporative fluid cooler air inlet wet-bulb temperature.");
                    ShowContinueError("Plant:Sizing object inputs also influence these results (e.g. DeltaT and ExitTemp).");
                    ShowContinueError("Inputs to the evaporative fluid cooler object:");
                    ShowContinueError("Design Evaporative Fluid Cooler Load [W]                      = " + General::RoundSigDigits(Par(1), 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Water Volume Flow Rate [m3/s] = " +
                                      General::RoundSigDigits(this->DesignWaterFlowRate, 6));
                    ShowContinueError("Design Evaporative Fluid Cooler Air Volume Flow Rate [m3/s]   = " + General::RoundSigDigits(Par(4), 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Air Inlet Wet-bulb Temp [C]   = " +
                                      General::RoundSigDigits(this->inletConds.AirWetBulb, 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Water Inlet Temp [C]          = " +
                                      General::RoundSigDigits(this->inletConds.WaterTemp, 2));
                    ShowContinueError("Inputs to the plant sizing object:");
                    ShowContinueError("Design Exit Water Temp [C]                                    = " +
                                      General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).ExitTemp, 2));
                    ShowContinueError("Loop Design Temperature Difference [C]                        = " +
                                      General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).DeltaT, 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Water Inlet Temp [C]          = " +
                                      General::RoundSigDigits(this->inletConds.WaterTemp, 2));
                    ShowContinueError("Calculated water outlet temperature at low UA [C](UA = " + General::RoundSigDigits(UA0, 2) +
                                      " W/C)  = " + General::RoundSigDigits(OutWaterTempAtUA0, 2));
                    ShowContinueError("Calculated water outlet temperature at high UA [C](UA = " + General::RoundSigDigits(UA1, 2) +
                                      " W/C)  = " + General::RoundSigDigits(OutWaterTempAtUA1, 2));
                    ShowFatalError("Autosizing of Evaporative Fluid Cooler UA failed for Evaporative Fluid Cooler = " +
                                   this->Name);
                }
                this->HighSpeedEvapFluidCoolerUA = UA;
            } else {
                this->HighSpeedEvapFluidCoolerUA = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (this->EvapFluidCoolerType_Num == EvapFluidCooler::SingleSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "U-Factor Times Area Value at Design Air Flow Rate [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_SingleSpeed,
                                           this->Name,
                                           "Initial U-Factor Times Area Value at Design Air Flow Rate [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                } else if (this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
                    if (DataPlant::PlantFinalSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "U-Factor Times Area Value at High Fan Speed [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                    if (DataPlant::PlantFirstSizesOkayToReport) {
                        ReportSizingManager::ReportSizingOutput(cEvapFluidCooler_TwoSpeed,
                                           this->Name,
                                           "Initial U-Factor Times Area Value at High Fan Speed [W/C]",
                                           this->HighSpeedEvapFluidCoolerUA);
                    }
                }
            }
        }

        if (this->LowSpeedAirFlowRateWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
            this->LowSpeedAirFlowRate =
                this->LowSpeedAirFlowRateSizingFactor *
                this->HighSpeedAirFlowRate;
            if (DataPlant::PlantFinalSizesOkayToReport) {
                ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                   this->Name,
                                   "Air Flow Rate at Low Fan Speed [m3/s]",
                                   this->LowSpeedAirFlowRate);
            }
            if (DataPlant::PlantFirstSizesOkayToReport) {
                ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                   this->Name,
                                   "Initial Air Flow Rate at Low Fan Speed [m3/s]",
                                   this->LowSpeedAirFlowRate);
            }
        }

        if (this->LowSpeedFanPowerWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
            this->LowSpeedFanPower =
                this->LowSpeedFanPowerSizingFactor * this->HighSpeedFanPower;
            if (DataPlant::PlantFinalSizesOkayToReport) {
                ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                   this->Name,
                                   "Fan Power at Low Fan Speed [W]",
                                   this->LowSpeedFanPower);
            }
            if (DataPlant::PlantFirstSizesOkayToReport) {
                ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                   this->Name,
                                   "Initial Fan Power at Low Fan Speed [W]",
                                   this->LowSpeedFanPower);
            }
        }

        if (this->LowSpeedEvapFluidCoolerUAWasAutoSized && DataPlant::PlantFirstSizesOkayToFinalize) {
            this->LowSpeedEvapFluidCoolerUA =
                this->LowSpeedEvapFluidCoolerUASizingFactor *
                this->HighSpeedEvapFluidCoolerUA;
            if (DataPlant::PlantFinalSizesOkayToReport) {
                ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                   this->Name,
                                   "U-Factor Times Area Value at Low Fan Speed [W/C]",
                                   this->LowSpeedEvapFluidCoolerUA);
            }
            if (DataPlant::PlantFirstSizesOkayToReport) {
                ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                   this->Name,
                                   "Initial U-Factor Times Area Value at Low Fan Speed [W/C]",
                                   this->LowSpeedEvapFluidCoolerUA);
            }
        }

        if (this->PerformanceInputMethod_Num == PIM::StandardDesignCapacity &&
            this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
            if (this->DesignWaterFlowRate >= DataHVACGlobals::SmallWaterVolFlow &&
                this->LowSpeedStandardDesignCapacity > 0.0) {
                // Standard design capacity doesn't include compressor heat;
                // predefined factor was 1.25 W heat rejection per W of delivered cooling, now user input with default 1.25
                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                       DataGlobals::InitConvTemp,
                                       DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                       CalledFrom);
                Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                           this->DesignEnteringWaterTemp,
                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                           CalledFrom);
                DesEvapFluidCoolerLoad = this->LowSpeedStandardDesignCapacity *
                                         this->HeatRejectCapNomCapSizingRatio;
                Par(1) = DesEvapFluidCoolerLoad;
                Par(2) = double(EvapFluidCoolerNum);
                Par(3) = rho * tmpDesignWaterFlowRate;                                  // Design water mass flow rate
                Par(4) = this->LowSpeedAirFlowRate; // Air volume flow rate at low fan speed
                Par(5) = Cp;
                Real64 UA0 = 0.0001 * DesEvapFluidCoolerLoad;                            // Assume deltaT = 10000K (limit)
                Real64 UA1 = DesEvapFluidCoolerLoad;                                     // Assume deltaT = 1K
                this->inletConds.WaterTemp = 35.0;  // 95F design inlet water temperature
                this->inletConds.AirTemp = 35.0;    // 95F design inlet air dry-bulb temp
                this->inletConds.AirWetBulb = 25.6; // 78F design inlet air wet-bulb temp
                this->inletConds.AirPress = DataEnvironment::StdBaroPress;
                this->inletConds.AirHumRat = Psychrometrics::PsyWFnTdbTwbPb(this->inletConds.AirTemp,
                                                                                          this->inletConds.AirWetBulb,
                                                                                          this->inletConds.AirPress);
                General::SolveRoot(Acc, MaxIte, SolFla, UA, SimpleEvapFluidCoolerUAResidual, UA0, UA1, Par);
                if (SolFla == -1) {
                    ShowWarningError("Iteration limit exceeded in calculating evaporative fluid cooler UA.");
                    ShowContinueError("Autosizing of fluid cooler UA failed for evaporative fluid cooler = " +
                                      this->Name);
                    ShowContinueError("The final UA value = " + General::RoundSigDigits(UA, 2) + "W/C, and the simulation continues...");
                } else if (SolFla == -2) {
                    ShowSevereError(CalledFrom + ": The combination of design input values did not allow the calculation of a ");
                    ShowContinueError("reasonable low-speed UA value. Review and revise design input values as appropriate. ");
                    ShowFatalError("Autosizing of Evaporative Fluid Cooler UA failed for Evaporative Fluid Cooler = " +
                                   this->Name);
                }
                this->LowSpeedEvapFluidCoolerUA = UA;
            } else {
                this->LowSpeedEvapFluidCoolerUA = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                       this->Name,
                                       "U-Factor Times Area Value at Low Fan Speed [W/C]",
                                       this->LowSpeedEvapFluidCoolerUA);
                }
                if (DataPlant::PlantFirstSizesOkayToReport) {
                    ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                       this->Name,
                                       "Initial U-Factor Times Area Value at Low Fan Speed [W/C]",
                                       this->LowSpeedEvapFluidCoolerUA);
                }
            }
        }

        if (this->PerformanceInputMethod_Num == PIM::UserSpecifiedDesignCapacity &&
            this->EvapFluidCoolerType_Num == EvapFluidCooler::TwoSpeed) {
            if (this->DesignWaterFlowRate >= DataHVACGlobals::SmallWaterVolFlow &&
                this->LowSpeedUserSpecifiedDesignCapacity > 0.0) {
                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                       DataGlobals::InitConvTemp,
                                       DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                       CalledFrom);
                Real64 Cp = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                           this->DesignEnteringWaterTemp,
                                           DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                           CalledFrom);
                DesEvapFluidCoolerLoad = this->LowSpeedUserSpecifiedDesignCapacity;
                Par(1) = DesEvapFluidCoolerLoad;
                Par(2) = double(EvapFluidCoolerNum);
                Par(3) = rho * tmpDesignWaterFlowRate;                                  // Design water mass flow rate
                Par(4) = this->LowSpeedAirFlowRate; // Air volume flow rate at low fan speed
                Par(5) = Cp;
                Real64 UA0 = 0.0001 * DesEvapFluidCoolerLoad; // Assume deltaT = 10000K (limit)
                Real64 UA1 = DesEvapFluidCoolerLoad;          // Assume deltaT = 1K
                this->inletConds.WaterTemp = this->DesignEnteringWaterTemp;
                this->inletConds.AirTemp = this->DesignEnteringAirTemp;
                this->inletConds.AirWetBulb = this->DesignEnteringAirWetBulbTemp;
                this->inletConds.AirPress = DataEnvironment::StdBaroPress;
                this->inletConds.AirHumRat = Psychrometrics::PsyWFnTdbTwbPb(this->inletConds.AirTemp,
                                                                                          this->inletConds.AirWetBulb,
                                                                                          this->inletConds.AirPress);
                General::SolveRoot(Acc, MaxIte, SolFla, UA, SimpleEvapFluidCoolerUAResidual, UA0, UA1, Par);
                if (SolFla == -1) {
                    ShowSevereError("Iteration limit exceeded in calculating EvaporativeFluidCooler UA");
                    ShowFatalError("Autosizing of EvaporativeFluidCooler UA failed for EvaporativeFluidCooler " +
                                   this->Name);
                } else if (SolFla == -2) {
                    SimSimpleEvapFluidCooler(int(Par(2)), Par(3), Par(4), UA0, OutWaterTempAtUA0);
                    SimSimpleEvapFluidCooler(int(Par(2)), Par(3), Par(4), UA1, OutWaterTempAtUA1);
                    ShowSevereError(CalledFrom + ": The combination of design input values did not allow the calculation of a ");
                    ShowContinueError("reasonable UA value. Review and revise design input values as appropriate. Specifying hard");
                    ShowContinueError(
                        R"(sizes for some "autosizable" fields while autosizing other "autosizable" fields may be contributing to this problem.)");
                    ShowContinueError("This model iterates on UA to find the heat transfer required to provide the design outlet ");
                    ShowContinueError("water temperature. Initially, the outlet water temperatures at high and low UA values are ");
                    ShowContinueError("calculated. The Design Exit Water Temperature should be between the outlet water ");
                    ShowContinueError("temperatures calculated at high and low UA values. If the Design Exit Water Temperature is ");
                    ShowContinueError("out of this range, the solution will not converge and UA will not be calculated. ");
                    ShowContinueError("Inputs to the Evaporative Fluid Cooler model are:");
                    ShowContinueError("Design Evaporative Fluid Cooler Load                    = " + General::RoundSigDigits(Par(1), 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Water Volume Flow Rate  = " + General::RoundSigDigits(Par(3), 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Air Volume Flow Rate    = " + General::RoundSigDigits(Par(4), 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Air Inlet Wet-bulb Temp = " +
                                      General::RoundSigDigits(this->inletConds.AirWetBulb, 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Water Inlet Temp        = " +
                                      General::RoundSigDigits(this->inletConds.WaterTemp, 2));
                    ShowContinueError("Design Exit Water Temp                                  = " +
                                      General::RoundSigDigits(DataSizing::PlantSizData(PltSizCondNum).ExitTemp, 2));
                    ShowContinueError("Design Evaporative Fluid Cooler Water Inlet Temp [C]    = " +
                                      General::RoundSigDigits(this->inletConds.WaterTemp, 2));
                    ShowContinueError("Calculated water outlet temperature at low UA(" + General::RoundSigDigits(UA0, 2) +
                                      ")  = " + General::RoundSigDigits(OutWaterTempAtUA0, 2));
                    ShowContinueError("Calculated water outlet temperature at high UA(" + General::RoundSigDigits(UA1, 2) +
                                      ")  = " + General::RoundSigDigits(OutWaterTempAtUA1, 2));
                    ShowFatalError("Autosizing of Evaporative Fluid Cooler UA failed for Evaporative Fluid Cooler = " +
                                   this->Name);
                }
                this->LowSpeedEvapFluidCoolerUA = UA;
            } else {
                this->LowSpeedEvapFluidCoolerUA = 0.0;
            }
            if (DataPlant::PlantFirstSizesOkayToFinalize) {
                if (DataPlant::PlantFinalSizesOkayToReport) {
                    ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                       this->Name,
                                       "U-Factor Times Area Value at Low Fan Speed [W/C]",
                                       this->LowSpeedEvapFluidCoolerUA);
                }
                if (DataPlant::PlantFirstSizesOkayToReport) {
                    ReportSizingManager::ReportSizingOutput(this->EvapFluidCoolerType,
                                       this->Name,
                                       "Initial U-Factor Times Area Value at Low Fan Speed [W/C]",
                                       this->LowSpeedEvapFluidCoolerUA);
                }
            }
        }

        if (DataPlant::PlantFinalSizesOkayToReport) {
            // create predefined report
            std::string equipName = this->Name;
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechType, equipName, this->EvapFluidCoolerType);
            OutputReportPredefined::PreDefTableEntry(OutputReportPredefined::pdchMechNomCap, equipName, this->HighSpeedStandardDesignCapacity);
        }
    }

    void EvapFluidCoolerSpecs::CalcSingleSpeedEvapFluidCooler(int &EvapFluidCoolerNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // To simulate the operation of a single-speed fan evaporative fluid cooler.

        // METHODOLOGY EMPLOYED:
        // The evaporative fluid cooler is modeled using effectiveness-NTU relationships for
        // counterflow heat exchangers based on Merkel's theory.
        // The subroutine calculates the period of time required to meet a
        // leaving water temperature setpoint. It assumes that part-load
        // operation represents a linear interpolation of two steady-state regimes.
        // Cyclic losses are neglected. The period of time required to meet the
        // leaving water temperature setpoint is used to determine the required
        // fan power and energy.
        // A RunFlag is passed by the upper level manager to indicate the ON/OFF status,
        // or schedule, of the evaporative fluid cooler. If the evaporative fluid cooler is OFF, outlet water
        // temperature and flow rate are passed through the model from inlet node to
        // outlet node without intervention. Reports are also updated with fan power and energy being zero.
        // When the RunFlag indicates an ON condition for the evaporative fluid cooler, the
        // mass flow rate and water temperature are read from the inlet node of the
        // evaporative fluid cooler (water-side). The outdoor air wet-bulb temperature is used
        // as the entering condition to the evaporative fluid cooler (air-side).
        // The evaporative fluid cooler fan is turned on and design parameters are used
        // to calculate the leaving water temperature.
        // If the calculated leaving water temperature is below the setpoint, a fan
        // run-time fraction is calculated and used to determine fan power. The leaving
        // water temperature setpoint is placed on the outlet node. If the calculated
        // leaving water temperature is at or above the setpoint, the calculated
        // leaving water temperature is placed on the outlet node and the fan runs at
        // full power. Water mass flow rate is passed from inlet node to outlet node
        // with no intervention.
        // REFERENCES:
        // ASHRAE HVAC1KIT: A Toolkit for Primary HVAC System Energy Calculation. 1999.

        // Based on SingleSpeedTower subroutine by Dan Fisher ,Sept 1998
        // Dec. 2008. BG. added RunFlag logic per original methodology

        std::string const RoutineName("CalcSingleSpeedEvapFluidCooler");
        int const MaxIteration(100); // Maximum fluid bypass iteration calculations
        std::string const MaxItChar("100");
        Real64 const BypassFractionThreshold(0.01); // Threshold to stop bypass iteration
        Real64 const OWTLowerLimit(0.0);            // The limit of evaporative fluid cooler exit fluid temperature used
        // in the fluid bypass calculation to avoid fluid freezing. For water,
        // it is 0 degreeC and for glycols, it can be much lower. The fluid type
        // is stored at the loop. Current choices are Water and Steam,
        // needs to expand for glycols

        Real64 AirFlowRate;
        Real64 UAdesign; // UA value at design conditions (entered by user or calculated)
        Real64 CpWater;
        Real64 TempSetPoint = 0.0;

        // set inlet and outlet nodes
        this->WaterInletNode = this->WaterInletNodeNum;
        this->WaterOutletNode = this->WaterOutletNodeNum;
        this->Qactual = 0.0;
        this->FanPower = 0.0;
        Real64 inletWaterTemp = DataLoopNode::Node(this->WaterInletNode).Temp;
        this->OutletWaterTemp = inletWaterTemp;
        int LoopNum = this->LoopNum;
        int LoopSideNum = this->LoopSideNum;
        AirFlowRate = 0.0;
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                TempSetPoint = DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).TempSetPoint;
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                TempSetPoint = DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).TempSetPointHi;
            }
        }

        // Added for fluid bypass. First assume no fluid bypass
        int BypassFlag = 0;
        this->BypassFraction = 0.0;
        int CapacityControl = this->CapacityControl;

        //   MassFlowTol is a parameter to indicate a no flow condition
        if (this->WaterMassFlowRate <= DataBranchAirLoopPlant::MassFlowTolerance || DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0) return;

        if (inletWaterTemp > TempSetPoint) {
            //     Turn on evaporative fluid cooler fan
            UAdesign = this->HighSpeedEvapFluidCoolerUA;
            AirFlowRate = this->HighSpeedAirFlowRate;
            Real64 FanPowerOn = this->HighSpeedFanPower;

            SimSimpleEvapFluidCooler(EvapFluidCoolerNum, this->WaterMassFlowRate, AirFlowRate, UAdesign, this->OutletWaterTemp);

            if (this->OutletWaterTemp <= TempSetPoint) {
                if (CapacityControl == 0 || this->OutletWaterTemp <= OWTLowerLimit) {
                    //         Setpoint was met with pump ON and fan ON, calculate run-time fraction
                    Real64 FanModeFrac = (TempSetPoint - inletWaterTemp) / (this->OutletWaterTemp - inletWaterTemp);
                    this->FanPower = FanModeFrac * FanPowerOn;
                    this->OutletWaterTemp = TempSetPoint;
                } else {
                    // FluidBypass, fan runs at full speed for the entire time step
                    // FanModeFrac = 1.0;
                    this->FanPower = FanPowerOn;
                    BypassFlag = 1;
                }
            } else {
                //       Setpoint was not met, evaporative fluid cooler ran at full capacity
                // FanModeFrac = 1.0;
                this->FanPower = FanPowerOn;
            }
        } else if (inletWaterTemp <= TempSetPoint) {
            // Inlet water temperature lower than setpoint, assume 100% bypass, evaporative fluid cooler fan off
            if (CapacityControl == 1) {
                if (inletWaterTemp > OWTLowerLimit) {
                    this->FanPower = 0.0;
                    this->BypassFraction = 1.0;
                    this->OutletWaterTemp = inletWaterTemp;
                }
            }
        }

        // Calculate bypass fraction since OWTLowerLimit < OutletWaterTemp < TempSetPoint.
        // The iteration ends when the numer of iteration exceeds the limit or the difference
        //  between the new and old bypass fractions is less than the threshold.
        if (BypassFlag == 1) {
            Real64 bypassFraction = (TempSetPoint - this->OutletWaterTemp) / (inletWaterTemp - this->OutletWaterTemp);
            if (bypassFraction > 1.0 || bypassFraction < 0.0) {
                // Bypass cannot meet setpoint, assume no bypass
                this->BypassFraction = 0.0;
                AirFlowRate = 0.0;
            } else {
                int NumIteration = 0;
                Real64 BypassFraction2; // Fluid bypass fraction
                Real64 BypassFractionPrev = bypassFraction;
                Real64 OutletWaterTempPrev = this->OutletWaterTemp;
                while (NumIteration < MaxIteration) {
                    ++NumIteration;
                    // need to iterate for the new OutletWaterTemp while bypassing evaporative fluid cooler water
                    SimSimpleEvapFluidCooler(EvapFluidCoolerNum, this->WaterMassFlowRate * (1.0 - bypassFraction), AirFlowRate, UAdesign, this->OutletWaterTemp);
                    // Calc new bypassFraction based on the new OutletWaterTemp
                    if (std::abs(this->OutletWaterTemp - OWTLowerLimit) <= 0.01) {
                        BypassFraction2 = bypassFraction;
                        break;
                    } else if (this->OutletWaterTemp < OWTLowerLimit) {
                        // Set OutletWaterTemp = OWTLowerLimit, and use linear interpolation to calculate the bypassFraction
                        BypassFraction2 = BypassFractionPrev - (BypassFractionPrev - bypassFraction) * (OutletWaterTempPrev - OWTLowerLimit) /
                                                               (OutletWaterTempPrev - this->OutletWaterTemp);
                        SimSimpleEvapFluidCooler(
                            EvapFluidCoolerNum, this->WaterMassFlowRate * (1.0 - BypassFraction2), AirFlowRate, UAdesign, this->OutletWaterTemp);
                        if (this->OutletWaterTemp < OWTLowerLimit) {
                            // Use previous iteraction values
                            BypassFraction2 = BypassFractionPrev;
                            this->OutletWaterTemp = OutletWaterTempPrev;
                        }
                        break;
                    } else {
                        BypassFraction2 = (TempSetPoint - this->OutletWaterTemp) / (inletWaterTemp - this->OutletWaterTemp);
                    }
                    // Compare two bypassFraction to determine when to stop
                    if (std::abs(BypassFraction2 - bypassFraction) <= BypassFractionThreshold) break;
                    BypassFractionPrev = bypassFraction;
                    OutletWaterTempPrev = this->OutletWaterTemp;
                    bypassFraction = BypassFraction2;
                }
                if (NumIteration > MaxIteration) {
                    ShowWarningError("Evaporative fluid cooler fluid bypass iteration exceeds maximum limit of " + MaxItChar + " for " +
                                     this->Name);
                }
                this->BypassFraction = BypassFraction2;
                // may not meet TempSetPoint due to limit of evaporative fluid cooler outlet temp to OWTLowerLimit
                this->OutletWaterTemp = (1.0 - BypassFraction2) * this->OutletWaterTemp + BypassFraction2 * inletWaterTemp;
            }
        }

        // Should this be water inlet node num?????
        CpWater = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                        DataLoopNode::Node(this->WaterInletNode).Temp,
                                        DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                        RoutineName);
        this->Qactual = this->WaterMassFlowRate * CpWater * (DataLoopNode::Node(this->WaterInletNode).Temp - this->OutletWaterTemp);
        this->AirFlowRateRatio = AirFlowRate / this->HighSpeedAirFlowRate;
    }

    void EvapFluidCoolerSpecs::CalcTwoSpeedEvapFluidCooler(int &EvapFluidCoolerNum)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // To simulate the operation of a evaporative fluid cooler with a two-speed fan.

        // METHODOLOGY EMPLOYED:
        // The evaporative fluid cooler is modeled using effectiveness-NTU relationships for
        // counterflow heat exchangers based on Merkel's theory.
        // The subroutine calculates the period of time required to meet a
        // leaving water temperature setpoint. It assumes that part-load
        // operation represents a linear interpolation of three steady-state regimes
        // (high-speed fan operation and low-speed fan operation ).
        // Cyclic losses are neglected. The period of time required to meet the
        // leaving water temperature setpoint is used to determine the required
        // fan power and energy. When the leaving water temperature is at or above the setpoint
        // the evaporative fluid cooler fan is turned on,
        // .
        // A RunFlag is passed by the upper level manager to indicate the ON/OFF status,
        // or schedule, of the evaporative fluid cooler. If the evaporative fluid cooler is OFF, outlet water
        // temperature and flow rate are passed through the model from inlet node to
        // outlet node without intervention. Reports are also updated with fan power and fan energy being zero.
        // When the RunFlag indicates an ON condition for the evaporative fluid cooler, the
        // mass flow rate and water temperature are read from the inlet node of the
        // evaporative fluid cooler (water-side). The outdoor air wet-bulb temperature is used
        // as the entering condition to the evaporative fluid cooler (air-side). If the incoming
        // water temperature is above the setpoint, the evaporative fluid cooler fan is turned on
        // and parameters for low fan speed are used to again calculate the leaving
        // water temperature. If the calculated leaving water temperature is
        // below the setpoint, a fan run-time fraction (FanModeFrac) is calculated and
        // used to determine fan power. The leaving water temperature setpoint is placed
        // on the outlet node. If the calculated leaving water temperature is at or above
        // the setpoint, the evaporative fluid cooler fan is turned on 'high speed' and the routine is
        // repeated. If the calculated leaving water temperature is below the setpoint,
        // a fan run-time fraction is calculated for the second stage fan and fan power
        // is calculated as FanModeFrac*HighSpeedFanPower+(1-FanModeFrac)*LowSpeedFanPower.
        // If the calculated leaving water temperature is above the leaving water temp.
        // setpoint, the calculated leaving water temperature is placed on the outlet
        // node and the fan runs at full power (High Speed Fan Power). Water mass flow
        // rate is passed from inlet node to outlet node with no intervention.
        // REFERENCES:
        // ASHRAE HVAC1KIT: A Toolkit for Primary HVAC System Energy Calculation. 1999.
        // Based on TwoSpeedTower by Dan Fisher ,Sept. 1998
        // Dec. 2008. BG. added RunFlag logic per original methodology

        std::string const RoutineName("CalcTwoSpeedEvapFluidCooler");

        this->WaterInletNode = this->WaterInletNodeNum;
        this->WaterOutletNode = this->WaterOutletNodeNum;
        this->Qactual = 0.0;
        this->FanPower = 0.0;
        this->InletWaterTemp = DataLoopNode::Node(this->WaterInletNode).Temp;
        this->OutletWaterTemp = this->InletWaterTemp;

        Real64 OutletWaterTemp1stStage = this->OutletWaterTemp;
        Real64 OutletWaterTemp2ndStage = this->OutletWaterTemp;
        Real64 AirFlowRate = 0.0;
        int LoopNum = this->LoopNum;
        int LoopSideNum = this->LoopSideNum;
        Real64 TempSetPoint = 0.0;
        {
            auto const SELECT_CASE_var(DataPlant::PlantLoop(LoopNum).LoopDemandCalcScheme);
            if (SELECT_CASE_var == DataPlant::SingleSetPoint) {
                TempSetPoint = DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).TempSetPoint;
            } else if (SELECT_CASE_var == DataPlant::DualSetPointDeadBand) {
                TempSetPoint = DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).TempSetPointHi;
            }
        }

        //   MassFlowTol is a parameter to indicate a no flow condition
        if (this->WaterMassFlowRate <= DataBranchAirLoopPlant::MassFlowTolerance || DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0) return;

        if (this->InletWaterTemp > TempSetPoint) {
            //     Setpoint was not met ,turn on evaporative fluid cooler 1st stage fan
            Real64 UAdesign = this->LowSpeedEvapFluidCoolerUA;
            AirFlowRate = this->LowSpeedAirFlowRate;
            Real64 FanPowerLow = this->LowSpeedFanPower;
            Real64 FanPowerHigh;
            SimSimpleEvapFluidCooler(EvapFluidCoolerNum, this->WaterMassFlowRate, AirFlowRate, UAdesign, OutletWaterTemp1stStage);

            if (OutletWaterTemp1stStage <= TempSetPoint) {
                //         Setpoint was met with pump ON and fan ON 1st stage, calculate fan mode fraction
                Real64 FanModeFrac = (TempSetPoint - this->InletWaterTemp) / (OutletWaterTemp1stStage - this->InletWaterTemp);
                this->FanPower = FanModeFrac * FanPowerLow;
                this->OutletWaterTemp = TempSetPoint;
                this->Qactual *= FanModeFrac;
            } else {
                //         Setpoint was not met, turn on evaporative fluid cooler 2nd stage fan
                UAdesign = this->HighSpeedEvapFluidCoolerUA;
                AirFlowRate = this->HighSpeedAirFlowRate;
                FanPowerHigh = this->HighSpeedFanPower;

                SimSimpleEvapFluidCooler(EvapFluidCoolerNum, this->WaterMassFlowRate, AirFlowRate, UAdesign, OutletWaterTemp2ndStage);

                if ((OutletWaterTemp2ndStage <= TempSetPoint) && UAdesign > 0.0) {
                    //           Setpoint was met with pump ON and fan ON 2nd stage, calculate fan mode fraction
                    Real64 FanModeFrac = (TempSetPoint - OutletWaterTemp1stStage) / (OutletWaterTemp2ndStage - OutletWaterTemp1stStage);
                    this->FanPower = (FanModeFrac * FanPowerHigh) + (1.0 - FanModeFrac) * FanPowerLow;
                    this->OutletWaterTemp = TempSetPoint;
                } else {
                    //           Setpoint was not met, evaporative fluid cooler ran at full capacity
                    this->OutletWaterTemp = OutletWaterTemp2ndStage;
                    this->FanPower = FanPowerHigh;
                }
            }
        }

        // Should this be water inlet node num??
        Real64 CpWater = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                        DataLoopNode::Node(this->WaterInletNode).Temp,
                                        DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                        RoutineName);
        this->Qactual = this->WaterMassFlowRate * CpWater * (DataLoopNode::Node(this->WaterInletNode).Temp - this->OutletWaterTemp);
        this->AirFlowRateRatio = AirFlowRate / this->HighSpeedAirFlowRate;
    }

    void SimSimpleEvapFluidCooler(
            int const EvapFluidCoolerNum, Real64 const waterMassFlowRate, Real64 const AirFlowRate, Real64 const UAdesign, Real64 &outletWaterTemp)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // See purpose for single speed or Two speed evaporative fluid cooler model

        // METHODOLOGY EMPLOYED:
        // See methodology for single speed or two speed evaporative fluid cooler model

        // REFERENCES:
        // Based on SimTower subroutine by Dan Fisher Sept. 1998
        // Merkel, F. 1925.  Verduftungskuhlung. VDI Forschungsarbeiten, Nr 275, Berlin.
        // ASHRAE     1999.  HVAC1KIT: A Toolkit for Primary HVAC System Energy Calculations.

        int const IterMax(50);                  // Maximum number of iterations allowed
        Real64 const WetBulbTolerance(0.00001); // Maximum error for exiting wet-bulb temperature between iterations
        Real64 const DeltaTwbTolerance(0.001); // Maximum error (tolerance) in DeltaTwb for iteration convergence [C]
        std::string const RoutineName("SimSimpleEvapFluidCooler");

        SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterInletNode = SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterInletNodeNum;
        SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterOutletNode = SimpleEvapFluidCooler(EvapFluidCoolerNum).WaterOutletNodeNum;
        Real64 qActual = 0.0;
        Real64 WetBulbError = 1.0;
        Real64 DeltaTwb = 1.0;

        // set local evaporative fluid cooler inlet and outlet temperature variables
        SimpleEvapFluidCooler(EvapFluidCoolerNum).InletWaterTemp = SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.WaterTemp;
        outletWaterTemp = SimpleEvapFluidCooler(EvapFluidCoolerNum).InletWaterTemp;
        Real64 InletAirTemp = SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirTemp;
        Real64 InletAirWetBulb = SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirWetBulb;

        if (UAdesign == 0.0) return;

        // set water and air properties
        Real64 AirDensity = Psychrometrics::PsyRhoAirFnPbTdbW(
            SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirPress, InletAirTemp, SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirHumRat);
        Real64 AirMassFlowRate = AirFlowRate * AirDensity;
        Real64 CpAir = Psychrometrics::PsyCpAirFnWTdb(SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirHumRat, InletAirTemp);
        Real64 CpWater = FluidProperties::GetSpecificHeatGlycol(DataPlant::PlantLoop(SimpleEvapFluidCooler(EvapFluidCoolerNum).LoopNum).FluidName,
                                                         SimpleEvapFluidCooler(EvapFluidCoolerNum).InletWaterTemp,
                                        DataPlant::PlantLoop(SimpleEvapFluidCooler(EvapFluidCoolerNum).LoopNum).FluidIndex,
                                        RoutineName);
        Real64 InletAirEnthalpy = Psychrometrics::PsyHFnTdbRhPb(InletAirWetBulb, 1.0, SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirPress);

        // initialize exiting wet bulb temperature before iterating on final solution
        Real64 OutletAirWetBulb = InletAirWetBulb + 6.0;

        // Calculate mass flow rates
        Real64 MdotCpWater = waterMassFlowRate * CpWater;
        int Iter = 0;
        while ((WetBulbError > WetBulbTolerance) && (Iter <= IterMax) && (DeltaTwb > DeltaTwbTolerance)) {
            ++Iter;
            Real64 OutletAirEnthalpy = Psychrometrics::PsyHFnTdbRhPb(OutletAirWetBulb, 1.0, SimpleEvapFluidCooler(EvapFluidCoolerNum).inletConds.AirPress);
            // calculate the airside specific heat and capacity
            Real64 CpAirside = (OutletAirEnthalpy - InletAirEnthalpy) / (OutletAirWetBulb - InletAirWetBulb);
            Real64 AirCapacity = AirMassFlowRate * CpAirside;
            // calculate the minimum to maximum capacity ratios of airside and waterside
            Real64 CapacityRatioMin = min(AirCapacity, MdotCpWater);
            Real64 CapacityRatioMax = max(AirCapacity, MdotCpWater);
            Real64 CapacityRatio = CapacityRatioMin / CapacityRatioMax;
            // Calculate heat transfer coefficient and number of transfer units (NTU)
            Real64 UAactual = UAdesign * CpAirside / CpAir;
            Real64 NumTransferUnits = UAactual / CapacityRatioMin;
            // calculate heat exchanger effectiveness
            Real64 effectiveness;
            if (CapacityRatio <= 0.995) {
                effectiveness = (1.0 - std::exp(-1.0 * NumTransferUnits * (1.0 - CapacityRatio))) /
                                (1.0 - CapacityRatio * std::exp(-1.0 * NumTransferUnits * (1.0 - CapacityRatio)));
            } else {
                effectiveness = NumTransferUnits / (1.0 + NumTransferUnits);
            }
            // calculate water to air heat transfer and store last exiting WB temp of air
            qActual = effectiveness * CapacityRatioMin * (SimpleEvapFluidCooler(EvapFluidCoolerNum).InletWaterTemp - InletAirWetBulb);
            Real64 OutletAirWetBulbLast = OutletAirWetBulb;
            // calculate new exiting wet bulb temperature of airstream
            OutletAirWetBulb = InletAirWetBulb + qActual / AirCapacity;
            // Check error tolerance and exit if satisfied
            DeltaTwb = std::abs(OutletAirWetBulb - InletAirWetBulb);
            // Add KelvinConv to denominator below convert OutletAirWetBulbLast to Kelvin to avoid divide by zero.
            // Wet bulb error units are delta K/K
            WetBulbError = std::abs((OutletAirWetBulb - OutletAirWetBulbLast) / (OutletAirWetBulbLast + DataGlobals::KelvinConv));
        }

        if (qActual >= 0.0) {
            outletWaterTemp = SimpleEvapFluidCooler(EvapFluidCoolerNum).InletWaterTemp - qActual / MdotCpWater;
        } else {
            outletWaterTemp = SimpleEvapFluidCooler(EvapFluidCoolerNum).InletWaterTemp;
        }
    }

    Real64 SimpleEvapFluidCoolerUAResidual(Real64 const UA,          // UA of evaporative fluid cooler
                                           Array1<Real64> const &Par // par(1) = design evaporative fluid cooler load [W]
    )
    {

        // FUNCTION INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS FUNCTION:
        // Calculates residual function (Design evaporative fluid cooler load - evaporative fluid cooler cooling output)
        //                                    / Design evaporative fluid cooler load.
        // Evaporative fluid cooler Cooling Output depends on the UA which is being varied to zero the residual.

        // METHODOLOGY EMPLOYED:
        // Puts UA into the evaporative fluid cooler data structure, calls SimSimpleEvapFluidCooler, and calculates
        // the residual as defined above.

        // REFERENCES:
        // Based on SimpleTowerUAResidual by Fred Buhl, May 2002

        Real64 Residuum; // residual to be minimized to zero

        // Locals
        // SUBROUTINE ARGUMENT DEFINITIONS:
        // par(2) = Evaporative fluid cooler number
        // par(3) = design water mass flow rate [kg/s]
        // par(4) = design air volume flow rate [m3/s]
        // par(5) = water specific heat [J/(kg*C)]

        int EvapFluidCoolerIndex; // index of this evaporative fluid cooler
        Real64 OutWaterTemp;      // outlet water temperature [C]
        Real64 CoolingOutput;     // Evaporative fluid cooler cooling output [W]

        EvapFluidCoolerIndex = int(Par(2));
        SimSimpleEvapFluidCooler(EvapFluidCoolerIndex, Par(3), Par(4), UA, OutWaterTemp);
        CoolingOutput = Par(5) * Par(3) * (SimpleEvapFluidCooler(EvapFluidCoolerIndex).inletConds.WaterTemp - OutWaterTemp);
        Residuum = (Par(1) - CoolingOutput) / Par(1);
        return Residuum;
    }

    void EvapFluidCoolerSpecs::CalculateWaterUseage()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Chandan Sharma
        //       DATE WRITTEN   May 2009
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Collect evaporative fluid cooler water useage calculations for
        // reuse by all the evaporative fluid cooler models.

        // METHODOLOGY EMPLOYED:
        // <description>

        // REFERENCES:
        // Based on CalculateWaterUseage subroutine for cooling tower by B. Griffith, August 2006

        std::string const RoutineName("CalculateWaterUseage");

        this->BlowdownVdot = 0.0;
        this->EvaporationVdot = 0.0;

        Real64 AverageWaterTemp = (this->InletWaterTemp + this->OutletWaterTemp) / 2.0;

        // Set water and air properties
        if (this->EvapLossMode == EvapLoss::ByMoistTheory) {

            Real64 AirDensity = Psychrometrics::PsyRhoAirFnPbTdbW(this->inletConds.AirPress,
                                           this->inletConds.AirTemp,
                                           this->inletConds.AirHumRat);
            Real64 AirMassFlowRate = this->AirFlowRateRatio * this->HighSpeedAirFlowRate * AirDensity;
            Real64 InletAirEnthalpy = Psychrometrics::PsyHFnTdbRhPb(
                this->inletConds.AirWetBulb, 1.0, this->inletConds.AirPress);

            if (AirMassFlowRate > 0.0) {
                // Calculate outlet air conditions for determining water usage

                Real64 OutletAirEnthalpy = InletAirEnthalpy + this->Qactual / AirMassFlowRate;
                Real64 OutletAirTSat = Psychrometrics::PsyTsatFnHPb(OutletAirEnthalpy, this->inletConds.AirPress);
                Real64 OutletAirHumRatSat = Psychrometrics::PsyWFnTdbH(OutletAirTSat, OutletAirEnthalpy);

                // calculate specific humidity ratios (HUMRAT to mass of moist air not dry air)
                Real64 InSpecificHumRat =
                    this->inletConds.AirHumRat / (1 + this->inletConds.AirHumRat);
                Real64 OutSpecificHumRat = OutletAirHumRatSat / (1 + OutletAirHumRatSat);

                // calculate average air temp for density call
                Real64 TairAvg = (this->inletConds.AirTemp + OutletAirTSat) / 2.0;

                // Amount of water evaporated
                Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                       TairAvg,
                                       DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                       RoutineName);
                this->EvaporationVdot = (AirMassFlowRate * (OutSpecificHumRat - InSpecificHumRat)) / rho; // [m3/s]
                if (this->EvaporationVdot < 0.0) this->EvaporationVdot = 0.0;
            } else {
                this->EvaporationVdot = 0.0;
            }

        } else if (this->EvapLossMode == EvapLoss::ByUserFactor) {
            Real64 rho = FluidProperties::GetDensityGlycol(DataPlant::PlantLoop(this->LoopNum).FluidName,
                                   AverageWaterTemp,
                                   DataPlant::PlantLoop(this->LoopNum).FluidIndex,
                                   RoutineName);
            this->EvaporationVdot = this->UserEvapLossFactor * (this->InletWaterTemp - this->OutletWaterTemp) * (this->WaterMassFlowRate / rho);
            if (this->EvaporationVdot < 0.0) this->EvaporationVdot = 0.0;
        } else {
            // should never come here
        }

        //   amount of water lost due to drift
        this->DriftVdot = this->DesignSprayWaterFlowRate * this->DriftLossFraction *
                this->AirFlowRateRatio;

        if (this->BlowdownMode == Blowdown::BySchedule) {
            // Amount of water lost due to blow down (purging contaminants from evaporative fluid cooler basin)
            if (this->SchedIDBlowdown > 0) {
                this->BlowdownVdot = ScheduleManager::GetCurrentScheduleValue(this->SchedIDBlowdown);
            } else {
                this->BlowdownVdot = 0.0;
            }
        } else if (this->BlowdownMode == Blowdown::ByConcentration) {
            if (this->ConcentrationRatio > 2.0) { // protect divide by zero
                this->BlowdownVdot = this->EvaporationVdot / (this->ConcentrationRatio - 1) - this->DriftVdot;
            } else {
                this->BlowdownVdot = this->EvaporationVdot - this->DriftVdot;
            }
            if (this->BlowdownVdot < 0.0) this->BlowdownVdot = 0.0;
        } else {
            // should never come here
        }

        // Added for fluid bypass
        if (this->CapacityControl == 1) {
            if (this->EvapLossMode == EvapLoss::ByUserFactor)
                this->EvaporationVdot *= (1 - this->BypassFraction);
            this->DriftVdot *= (1 - this->BypassFraction);
            this->BlowdownVdot *= (1 - this->BypassFraction);
        }

        this->MakeUpVdot = this->EvaporationVdot + this->DriftVdot + this->BlowdownVdot;

        // set demand request in Water Storage if needed
        this->StarvedMakeUpVdot = 0.0;
        this->TankSupplyVdot = 0.0;
        if (this->SuppliedByWaterSystem) {

            // set demand request
            DataWater::WaterStorage(this->WaterTankID)
                .VdotRequestDemand(this->WaterTankDemandARRID) = this->MakeUpVdot;

            Real64 AvailTankVdot =
                DataWater::WaterStorage(this->WaterTankID)
                    .VdotAvailDemand(this->WaterTankDemandARRID); // check what tank can currently provide

            this->TankSupplyVdot = this->MakeUpVdot;      // init
            if (AvailTankVdot < this->MakeUpVdot) { // calculate starved flow
                this->StarvedMakeUpVdot = this->MakeUpVdot - AvailTankVdot;
                this->TankSupplyVdot = AvailTankVdot;
            }
        } else { // supplied by mains
        }

        //   total water usage
        // update report variables
        this->EvaporationVol = this->EvaporationVdot * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
        this->DriftVol = this->DriftVdot * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
        this->BlowdownVol = this->BlowdownVdot * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
        this->MakeUpVol = this->MakeUpVdot * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
        this->TankSupplyVol = this->TankSupplyVdot * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
        this->StarvedMakeUpVol = this->StarvedMakeUpVdot * (DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour);
    }

    void EvapFluidCoolerSpecs::UpdateEvapFluidCooler()
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Chandan Sharma
        //       DATE WRITTEN:    May 2009
        //       MODIFIED         na
        //       RE-ENGINEERED    na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine is for passing results to the outlet water node.

        ObjexxFCL::gio::Fmt LowTempFmt("(' ',F6.2)");
        Real64 const TempAllowance(0.02); // Minimum difference b/w fluid cooler water outlet temp and
        std::string CharErrOut;
        std::string CharLowOutletTemp;

        DataLoopNode::Node(this->WaterOutletNode).Temp = this->OutletWaterTemp;

        int LoopNum = this->LoopNum;
        int LoopSideNum = this->LoopSideNum;
        if (DataPlant::PlantLoop(LoopNum).LoopSide(LoopSideNum).FlowLock == 0 || DataGlobals::WarmupFlag) return;

        // Check flow rate through evaporative fluid cooler and compare to design flow rate,
        // show warning if greater than Design * Mulitplier
        if (DataLoopNode::Node(this->WaterOutletNode).MassFlowRate > this->DesWaterMassFlowRate *
                                                     this->EvapFluidCoolerMassFlowRateMultiplier) {
            ++this->HighMassFlowErrorCount;
            if (this->HighMassFlowErrorCount < 2) {
                ShowWarningError(this->EvapFluidCoolerType + " \"" +
                                 this->Name + "\"");
                ShowContinueError(" Condenser Loop Mass Flow Rate is much greater than the evaporative fluid coolers design mass flow rate.");
                ShowContinueError(" Condenser Loop Mass Flow Rate = " + General::TrimSigDigits(DataLoopNode::Node(this->WaterOutletNode).MassFlowRate, 6));
                ShowContinueError(" Evaporative Fluid Cooler Design Mass Flow Rate   = " +
                                  General::TrimSigDigits(this->DesWaterMassFlowRate, 6));
                ShowContinueErrorTimeStamp("");
            } else {
                ShowRecurringWarningErrorAtEnd(
                    this->EvapFluidCoolerType + " \"" + this->Name +
                        "\"  Condenser Loop Mass Flow Rate is much greater than the evaporative fluid coolers design mass flow rate error",
                    this->HighMassFlowErrorIndex,
                    DataLoopNode::Node(this->WaterOutletNode).MassFlowRate,
                    DataLoopNode::Node(this->WaterOutletNode).MassFlowRate);
            }
        }

        // Check if OutletWaterTemp is below the minimum condenser loop temp and warn user
        Real64 LoopMinTemp = DataPlant::PlantLoop(LoopNum).MinTemp;
        Real64 TempDifference = DataPlant::PlantLoop(LoopNum).MinTemp - this->OutletWaterTemp;
        if (TempDifference > TempAllowance && this->WaterMassFlowRate > 0.0) {
            ++this->OutletWaterTempErrorCount;
            ObjexxFCL::gio::write(CharLowOutletTemp, LowTempFmt) << LoopMinTemp;
            ObjexxFCL::gio::write(CharErrOut, LowTempFmt) << this->OutletWaterTemp;
            strip(CharErrOut);
            if (this->OutletWaterTempErrorCount < 2) {
                ShowWarningError(this->EvapFluidCoolerType + " \"" +
                                 this->Name + "\"");
                ShowContinueError("Evaporative fluid cooler water outlet temperature (" + CharErrOut +
                                  " C) is below the specified minimum condenser loop temp of " + stripped(CharLowOutletTemp) + " C");
                ShowContinueErrorTimeStamp("");
            } else {
                ShowRecurringWarningErrorAtEnd(
                    this->EvapFluidCoolerType + " \"" + this->Name +
                        "\" Evaporative fluid cooler water outlet temperature is below the specified minimum condenser loop temp error",
                    this->OutletWaterTempErrorIndex,
                    this->OutletWaterTemp,
                    this->OutletWaterTemp);
            }
        }

        // Check if water mass flow rate is small (e.g. no flow) and warn user
        if (this->WaterMassFlowRate > 0.0 && this->WaterMassFlowRate <= DataBranchAirLoopPlant::MassFlowTolerance) {
            ++this->SmallWaterMassFlowErrorCount;
            if (this->SmallWaterMassFlowErrorCount < 2) {
                ShowWarningError(this->EvapFluidCoolerType + " \"" +
                                 this->Name + "\"");
                ShowContinueError("Evaporative fluid cooler water mass flow rate near zero.");
                ShowContinueErrorTimeStamp("");
                ShowContinueError("Actual Mass flow = " + General::TrimSigDigits(this->WaterMassFlowRate, 2));
            } else {
                ShowRecurringWarningErrorAtEnd(this->EvapFluidCoolerType + " \"" +
                                                   this->Name +
                                                   "\" Evaporative fluid cooler water mass flow rate near zero error continues...",
                                               this->SmallWaterMassFlowErrorIndex,
                                               this->WaterMassFlowRate,
                                               this->WaterMassFlowRate);
            }
        }
    }

    void EvapFluidCoolerSpecs::ReportEvapFluidCooler(bool const RunFlag)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR:          Chandan Sharma
        //       DATE WRITTEN:    May 2009
        //       MODIFIED         na
        //       RE-ENGINEERED    na

        // PURPOSE OF THIS SUBROUTINE:
        // This subroutine updates the report variables for the evaporative fluid cooler.

        Real64 ReportingConstant;

        ReportingConstant = DataHVACGlobals::TimeStepSys * DataGlobals::SecInHour;

        if (!RunFlag) {
            this->fluidCoolerInletWaterTemp = DataLoopNode::Node(this->WaterInletNode).Temp;
            this->fluidCoolerOutletWaterTemp = DataLoopNode::Node(this->WaterInletNode).Temp;
            this->Qactual = 0.0;
            this->FanPower = 0.0;
            this->FanEnergy = 0.0;
            this->AirFlowRateRatio = 0.0;
            this->WaterAmountUsed = 0.0;
            this->BypassFraction = 0.0;
        } else {
            this->fluidCoolerInletWaterTemp = DataLoopNode::Node(this->WaterInletNode).Temp;
            this->fluidCoolerOutletWaterTemp = this->OutletWaterTemp;
            this->FanEnergy = this->FanPower * ReportingConstant;
            this->WaterAmountUsed = this->WaterUsage * ReportingConstant;
        }
    }

    void clear_state()
    {
        NumSimpleEvapFluidCoolers = 0;
        UniqueSimpleEvapFluidCoolerNames.clear();
        GetEvapFluidCoolerInputFlag = true;
    }

} // namespace EvaporativeFluidCoolers

} // namespace EnergyPlus
