// EnergyPlus, Copyright (c) 1996-2020, The Board of Trustees of the University of Illinois,
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
#include <cmath>

// ObjexxFCL Headers
#include <ObjexxFCL/Array1D.hh>
#include <ObjexxFCL/Fmath.hh>
#include <ObjexxFCL/member.functions.hh>

// EnergyPlus Headers
#include <EnergyPlus/Autosizing/Base.hh>
#include <EnergyPlus/ConvectionCoefficients.hh>
#include <EnergyPlus/DataEnvironment.hh>
#include <EnergyPlus/DataGlobals.hh>
#include <EnergyPlus/DataHVACGlobals.hh>
#include <EnergyPlus/DataHeatBalFanSys.hh>
#include <EnergyPlus/DataHeatBalSurface.hh>
#include <EnergyPlus/DataHeatBalance.hh>
#include <EnergyPlus/DataLoopNode.hh>
#include <EnergyPlus/DataRoomAirModel.hh>
#include <EnergyPlus/DataSizing.hh>
#include <EnergyPlus/DataSurfaces.hh>
#include <EnergyPlus/DataUCSDSharedData.hh>
#include <EnergyPlus/DataZoneEquipment.hh>
#include <EnergyPlus/Data/EnergyPlusData.hh>
#include <EnergyPlus/General.hh>
#include <EnergyPlus/InternalHeatGains.hh>
#include <EnergyPlus/Psychrometrics.hh>
#include <EnergyPlus/ScheduleManager.hh>
#include <EnergyPlus/UFADManager.hh>
#include <EnergyPlus/UtilityRoutines.hh>

namespace EnergyPlus {

namespace UFADManager {

    // Module containing the routines dealing with the UnderFloor Air
    // Distribution zone model

    // MODULE INFORMATION:
    //       AUTHOR         Fred Buhl
    //       DATE WRITTEN   August 2005
    //       MODIFIED       na
    //       RE-ENGINEERED  na

    // PURPOSE OF THIS MODULE:
    // Encapsulate the routines that do the simulation of the UCSD UFAD non-uniform
    // zone models

    // METHODOLOGY EMPLOYED:
    // 2-node zone model with the node heights varying as a function of internal loads
    // and supply air flow (and other factors)

    // REFERENCES:
    // See the EnergyPlus Engineering Reference and the PhD thesis of Anna Liu, UC San Diego

    // OTHER NOTES:
    // na

    // Using/Aliasing
    using namespace DataLoopNode;
    using namespace DataEnvironment;
    using namespace DataHeatBalance;
    using namespace DataHeatBalSurface;
    using namespace DataSurfaces;
    using namespace DataRoomAirModel;
    using ConvectionCoefficients::CalcDetailedHcInForDVModel;
    using DataHVACGlobals::PreviousTimeStep;
    using DataHVACGlobals::ShortenTimeStepSysRoomAir;
    using DataHVACGlobals::SysTimeElapsed;
    using namespace DataUCSDSharedData;

    // Data
    // MODULE VARIABLE DECLARATIONS:
    static std::string const BlankString;

    void ManageUCSDUFModels(EnergyPlusData &state,
                            int const ZoneNum,      // index number for the specified zone
                            int const ZoneModelType // type of zone model; UCSDUFI = 6
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   August, 2005
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Manages the simulation of the 2-node nonuniform zone models for underfloor air
        // distribution systems (UFAD). Called from RoomAirManager, ManageAirModel

        // METHODOLOGY EMPLOYED:
        // uses Init and Calc routines in the standard EPlus manner to manage the calculation
        // Note that much of the initialization is done in RoomAirManager, SharedDVCVUFDataInit

        // Using/Aliasing
        using namespace DataLoopNode;
        using namespace DataEnvironment;
        using namespace DataHeatBalance;
        using namespace DataHeatBalSurface;
        using namespace DataSurfaces;
        using namespace DataRoomAirModel;
        using ConvectionCoefficients::CalcDetailedHcInForDVModel;
        using namespace DataUCSDSharedData;

        // input was obtained in RoomAirManager, GetUFADIntZoneData

        InitUCSDUF(state, ZoneNum, ZoneModelType); // initialize some module variables

        {
            auto const SELECT_CASE_var(ZoneModelType);

            if (SELECT_CASE_var == RoomAirModel_UCSDUFI) { // UCSD UFAD interior zone model
                // simulate room airflow using the UCSDUFI model
                CalcUCSDUI(state, ZoneNum);

            } else if (SELECT_CASE_var == RoomAirModel_UCSDUFE) { // UCSD UFAD interior zone model
                // simulate room airflow using the UCSDUFI model
                CalcUCSDUE(state, ZoneNum);
            }
        }
    }

    void InitUCSDUF(EnergyPlusData &state, int const ZoneNum,
                    int const ZoneModelType // type of zone model; UCSDUFI = 6
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   August 2005
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // initialize arrays & variables used by the UCSD UFAD zone models

        // METHODOLOGY EMPLOYED:
        // Note that much of the initialization is done in RoomAirManager, SharedDVCVUFDataInit

        static Array1D_bool MySizeFlag;
        static Real64 NumShadesDown(0.0);
        int UINum;             // index to underfloor interior zone model data
        static int Ctd(0);     // DO loop index
        static int SurfNum(0); // surface data structure index

        // Do the one time initializations
        if (state.dataUFADManager->MyOneTimeFlag) {
            state.dataUFADManager->HeightFloorSubzoneTop = 0.2;
            state.dataUFADManager->ThickOccupiedSubzoneMin = 0.2;
            state.dataUFADManager->HeightIntMassDefault = 2.0;
            state.dataUFADManager->MyOneTimeFlag = false;
            MySizeFlag.dimension(state.dataGlobal->NumOfZones, true);
        }

        if (MySizeFlag(ZoneNum)) {
            SizeUCSDUF(state, ZoneNum, ZoneModelType);
            MySizeFlag(ZoneNum) = false;
        }

        // initialize these variables every timestep

        state.dataUFADManager->HeightIntMass = state.dataUFADManager->HeightIntMassDefault;
        ZoneUFGamma(ZoneNum) = 0.0;
        ZoneUFPowInPlumes(ZoneNum) = 0.0;
        NumShadesDown = 0.0;
        for (Ctd = PosZ_Window((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Window((ZoneNum - 1) * 2 + 2); ++Ctd) {
            SurfNum = APos_Window(Ctd);
            if (SurfNum == 0) continue;
            if (Surface(SurfNum).ExtBoundCond == ExternalEnvironment || Surface(SurfNum).ExtBoundCond == OtherSideCoefNoCalcExt ||
                Surface(SurfNum).ExtBoundCond == OtherSideCoefCalcExt || Surface(SurfNum).ExtBoundCond == OtherSideCondModeledExt) {
                if (SurfWinShadingFlag(SurfNum) == WinShadingFlag::IntShadeOn || SurfWinShadingFlag(SurfNum) == WinShadingFlag::IntBlindOn) {
                    ++NumShadesDown;
                }
            }
        }
        if (ZoneModelType == RoomAirModel_UCSDUFE) {
            UINum = ZoneUFPtr(ZoneNum);
            if (ZoneUCSDUE(UINum).NumExtWin > 1.0) {
                if (NumShadesDown / ZoneUCSDUE(UINum).NumExtWin >= 0.5) {
                    ZoneUCSDUE(UINum).ShadeDown = true;
                } else {
                    ZoneUCSDUE(UINum).ShadeDown = false;
                }
            } else {
                ZoneUCSDUE(UINum).ShadeDown = false;
            }
        }
    }

    void SizeUCSDUF(EnergyPlusData &state, int const ZoneNum,
                    int const ZoneModelType // type of zone model; UCSDUFI = 6
    )
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   August 2005
        //       MODIFIED       na
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // set some smart defaults for UFAD systems

        // METHODOLOGY EMPLOYED:
        // use data from Center for Built Environment

        using DataSizing::AutoSize;

        int UINum;                            // index to underfloor interior zone model data
        static int Ctd(0);                    // DO loop index
        static int SurfNum(0);                // surface data structure index
        static Real64 NumberOfOccupants(0.0); // design number of occupants in the zone
        static Real64 NumberOfPlumes(0.0);    // design number of plumes in the zone
        static Real64 ZoneElecConv(0.0);      // zone elec equip design convective gain [W]
        static Real64 ZoneGasConv(0.0);       // zone gas equip design convective gain [W]
        static Real64 ZoneOthEqConv(0.0);     // zone other equip design convective gain [W]
        static Real64 ZoneHWEqConv(0.0);      // zone hot water equip design convective gain [W]
        static Real64 ZoneSteamEqConv(0.0);   // zone steam equip design convective gain [W]

        if (ZoneModelType == RoomAirModel_UCSDUFI) {
            UINum = ZoneUFPtr(ZoneNum);
            NumberOfOccupants = 0.0;
            for (Ctd = 1; Ctd <= TotPeople; ++Ctd) {
                if (People(Ctd).ZonePtr == ZoneNum) {
                    NumberOfOccupants += People(Ctd).NumberOfPeople;
                }
            }
            if (ZoneUCSDUI(UINum).DiffArea == AutoSize) {
                if (ZoneUCSDUI(UINum).DiffuserType == Swirl) {
                    ZoneUCSDUI(UINum).DiffArea = 0.0075;
                } else if (ZoneUCSDUI(UINum).DiffuserType == VarArea) {
                    ZoneUCSDUI(UINum).DiffArea = 0.035;
                } else if (ZoneUCSDUI(UINum).DiffuserType == DisplVent) {
                    ZoneUCSDUI(UINum).DiffArea = 0.0060;
                } else if (ZoneUCSDUI(UINum).DiffuserType == LinBarGrille) {
                    // 4 ft x 4 inches; 75 cfm per linear foot; area is .025 m2/m
                    ZoneUCSDUI(UINum).DiffArea = 0.03;
                } else {
                    ZoneUCSDUI(UINum).DiffArea = 0.0075;
                }
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionInterior",
                                             ZoneUCSDUI(UINum).ZoneName,
                                             "Design effective area of diffuser",
                                             ZoneUCSDUI(UINum).DiffArea);
            }
            if (ZoneUCSDUI(UINum).DiffAngle == AutoSize) {
                if (ZoneUCSDUI(UINum).DiffuserType == Swirl) {
                    ZoneUCSDUI(UINum).DiffAngle = 28.0;
                } else if (ZoneUCSDUI(UINum).DiffuserType == VarArea) {
                    ZoneUCSDUI(UINum).DiffAngle = 45.0;
                } else if (ZoneUCSDUI(UINum).DiffuserType == DisplVent) {
                    ZoneUCSDUI(UINum).DiffAngle = 73.0;
                } else if (ZoneUCSDUI(UINum).DiffuserType == LinBarGrille) {
                    ZoneUCSDUI(UINum).DiffAngle = 15.0;
                } else {
                    ZoneUCSDUI(UINum).DiffAngle = 28.0;
                }
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionInterior",
                                             ZoneUCSDUI(UINum).ZoneName,
                                             "Angle between diffuser slots and the vertical",
                                             ZoneUCSDUI(UINum).DiffAngle);
            }
            if (ZoneUCSDUI(UINum).TransHeight == AutoSize) {
                ZoneUCSDUI(UINum).CalcTransHeight = true;
                ZoneUCSDUI(UINum).TransHeight = 0.0;
            } else {
                ZoneUCSDUI(UINum).CalcTransHeight = false;
            }
            if (ZoneUCSDUI(UINum).DiffuserType == Swirl) {
                if (ZoneUCSDUI(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUI(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionInterior for Zone " + ZoneUCSDUI(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = Swirl.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUI(UINum).A_Kc = 0.0;
                ZoneUCSDUI(UINum).B_Kc = 0.0;
                ZoneUCSDUI(UINum).C_Kc = 0.6531;
                ZoneUCSDUI(UINum).D_Kc = 0.0069;
                ZoneUCSDUI(UINum).E_Kc = -0.00004;
            } else if (ZoneUCSDUI(UINum).DiffuserType == VarArea) {
                if (ZoneUCSDUI(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUI(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionInterior for Zone " + ZoneUCSDUI(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = VariableArea.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUI(UINum).A_Kc = 0.0;
                ZoneUCSDUI(UINum).B_Kc = 0.0;
                ZoneUCSDUI(UINum).C_Kc = 0.88;
                ZoneUCSDUI(UINum).D_Kc = 0.0;
                ZoneUCSDUI(UINum).E_Kc = 0.0;
            } else if (ZoneUCSDUI(UINum).DiffuserType == DisplVent) {
                if (ZoneUCSDUI(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUI(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionInterior for Zone " + ZoneUCSDUI(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = HorizontalDisplacement.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUI(UINum).A_Kc = 0.0;
                ZoneUCSDUI(UINum).B_Kc = 0.0;
                ZoneUCSDUI(UINum).C_Kc = 0.67;
                ZoneUCSDUI(UINum).D_Kc = 0.0;
                ZoneUCSDUI(UINum).E_Kc = 0.0;
            } else if (ZoneUCSDUI(UINum).DiffuserType == LinBarGrille) {
                if (ZoneUCSDUI(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUI(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionInterior for Zone " + ZoneUCSDUI(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = LinearBarGrille.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUI(UINum).A_Kc = 0.0;
                ZoneUCSDUI(UINum).B_Kc = 0.0;
                ZoneUCSDUI(UINum).C_Kc = 0.8;
                ZoneUCSDUI(UINum).D_Kc = 0.0;
                ZoneUCSDUI(UINum).E_Kc = 0.0;
            } else {
                if (ZoneUCSDUI(UINum).A_Kc == DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).B_Kc == DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).C_Kc == DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUI(UINum).D_Kc == DataGlobalConstants::AutoCalculate() || ZoneUCSDUI(UINum).E_Kc == DataGlobalConstants::AutoCalculate()) {
                    ShowFatalError(state, "For RoomAirSettings:UnderFloorAirDistributionInterior for Zone " + ZoneUCSDUI(UINum).ZoneName +
                                   ", input for Coefficients A - E must be specified when Floor Diffuser Type = Custom.");
                }
            }
            if (ZoneUCSDUI(UINum).PowerPerPlume == DataGlobalConstants::AutoCalculate()) {
                NumberOfPlumes = 0.0;
                if (NumberOfOccupants > 0.0) {
                    NumberOfPlumes = NumberOfOccupants;
                } else {
                    NumberOfPlumes = 1.0;
                }
                ZoneElecConv = 0.0;
                for (Ctd = 1; Ctd <= TotElecEquip; ++Ctd) {
                    if (ZoneElectric(Ctd).ZonePtr == ZoneNum) {
                        ZoneElecConv += ZoneElectric(Ctd).DesignLevel * ZoneElectric(Ctd).FractionConvected;
                    }
                }
                ZoneGasConv = 0.0;
                for (Ctd = 1; Ctd <= TotGasEquip; ++Ctd) {
                    if (ZoneGas(Ctd).ZonePtr == ZoneNum) {
                        ZoneGasConv += ZoneGas(Ctd).DesignLevel * ZoneGas(Ctd).FractionConvected;
                    }
                }
                ZoneOthEqConv = 0.0;
                for (Ctd = 1; Ctd <= TotOthEquip; ++Ctd) {
                    if (ZoneOtherEq(Ctd).ZonePtr == ZoneNum) {
                        ZoneOthEqConv += ZoneOtherEq(Ctd).DesignLevel * ZoneOtherEq(Ctd).FractionConvected;
                    }
                }
                ZoneHWEqConv = 0.0;
                for (Ctd = 1; Ctd <= TotHWEquip; ++Ctd) {
                    if (ZoneHWEq(Ctd).ZonePtr == ZoneNum) {
                        ZoneHWEqConv += ZoneHWEq(Ctd).DesignLevel * ZoneHWEq(Ctd).FractionConvected;
                    }
                }
                for (Ctd = 1; Ctd <= TotStmEquip; ++Ctd) {
                    ZoneSteamEqConv = 0.0;
                    if (ZoneSteamEq(Ctd).ZonePtr == ZoneNum) {
                        ZoneSteamEqConv += ZoneSteamEq(Ctd).DesignLevel * ZoneSteamEq(Ctd).FractionConvected;
                    }
                }
                ZoneUCSDUI(UINum).PowerPerPlume =
                    (NumberOfOccupants * 73.0 + ZoneElecConv + ZoneGasConv + ZoneOthEqConv + ZoneHWEqConv + ZoneSteamEqConv) / NumberOfPlumes;
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionInterior",
                                             ZoneUCSDUI(UINum).ZoneName,
                                             "Power per plume [W]",
                                             ZoneUCSDUI(UINum).PowerPerPlume);
            }
            if (ZoneUCSDUI(UINum).DiffusersPerZone == AutoSize) {
                if (NumberOfOccupants > 0.0) {
                    ZoneUCSDUI(UINum).DiffusersPerZone = NumberOfOccupants;
                } else {
                    ZoneUCSDUI(UINum).DiffusersPerZone = 1.0;
                }
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionInterior",
                                             ZoneUCSDUI(UINum).ZoneName,
                                             "Number of diffusers per zone",
                                             ZoneUCSDUI(UINum).DiffusersPerZone);
            }
        }

        if (ZoneModelType == RoomAirModel_UCSDUFE) {
            UINum = ZoneUFPtr(ZoneNum);
            // calculate total window width in zone
            for (Ctd = PosZ_Window((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Window((ZoneNum - 1) * 2 + 2); ++Ctd) {
                SurfNum = APos_Window(Ctd);
                if (SurfNum == 0) continue;
                if (Surface(SurfNum).ExtBoundCond == ExternalEnvironment || Surface(SurfNum).ExtBoundCond == OtherSideCoefNoCalcExt ||
                    Surface(SurfNum).ExtBoundCond == OtherSideCoefCalcExt || Surface(SurfNum).ExtBoundCond == OtherSideCondModeledExt) {
                    ZoneUCSDUE(UINum).WinWidth += Surface(SurfNum).Width;
                    ++ZoneUCSDUE(UINum).NumExtWin;
                }
            }
            if (ZoneUCSDUE(UINum).WinWidth <= 0.0) {
                ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionExterior for Zone " + ZoneUCSDUE(UINum).ZoneName +
                                 " there are no exterior windows.");
                ShowContinueError(state, "  The zone will be treated as a UFAD interior zone");
            }
            NumberOfOccupants = 0.0;
            for (Ctd = 1; Ctd <= TotPeople; ++Ctd) {
                if (People(Ctd).ZonePtr == ZoneNum) {
                    NumberOfOccupants += People(Ctd).NumberOfPeople;
                }
            }
            if (ZoneUCSDUE(UINum).DiffArea == AutoSize) {
                if (ZoneUCSDUE(UINum).DiffuserType == Swirl) {
                    ZoneUCSDUE(UINum).DiffArea = 0.0075;
                } else if (ZoneUCSDUE(UINum).DiffuserType == VarArea) {
                    ZoneUCSDUE(UINum).DiffArea = 0.035;
                } else if (ZoneUCSDUE(UINum).DiffuserType == DisplVent) {
                    ZoneUCSDUE(UINum).DiffArea = 0.0060;
                } else if (ZoneUCSDUE(UINum).DiffuserType == LinBarGrille) {
                    // 4 ft x 4 inches; eff area is 50% of total area; 75 cfm per linear foot.
                    ZoneUCSDUE(UINum).DiffArea = 0.03;
                } else {
                    ZoneUCSDUE(UINum).DiffArea = 0.0075;
                }
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionExterior",
                                             ZoneUCSDUE(UINum).ZoneName,
                                             "Design effective area of diffuser",
                                             ZoneUCSDUE(UINum).DiffArea);
            }
            if (ZoneUCSDUE(UINum).DiffAngle == AutoSize) {
                if (ZoneUCSDUE(UINum).DiffuserType == Swirl) {
                    ZoneUCSDUE(UINum).DiffAngle = 28.0;
                } else if (ZoneUCSDUE(UINum).DiffuserType == VarArea) {
                    ZoneUCSDUE(UINum).DiffAngle = 45.0;
                } else if (ZoneUCSDUE(UINum).DiffuserType == DisplVent) {
                    ZoneUCSDUE(UINum).DiffAngle = 73.0;
                } else if (ZoneUCSDUE(UINum).DiffuserType == LinBarGrille) {
                    ZoneUCSDUE(UINum).DiffAngle = 15.0;
                } else {
                    ZoneUCSDUE(UINum).DiffAngle = 28.0;
                }
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionExterior",
                                             ZoneUCSDUE(UINum).ZoneName,
                                             "Angle between diffuser slots and the vertical",
                                             ZoneUCSDUE(UINum).DiffAngle);
            }
            if (ZoneUCSDUE(UINum).TransHeight == AutoSize) {
                ZoneUCSDUE(UINum).CalcTransHeight = true;
                ZoneUCSDUE(UINum).TransHeight = 0.0;
            } else {
                ZoneUCSDUE(UINum).CalcTransHeight = false;
            }
            if (ZoneUCSDUE(UINum).DiffuserType == Swirl) {
                if (ZoneUCSDUE(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUE(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionExterior for Zone " + ZoneUCSDUE(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = Swirl.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUE(UINum).A_Kc = 0.0;
                ZoneUCSDUE(UINum).B_Kc = 0.0;
                ZoneUCSDUE(UINum).C_Kc = 0.6531;
                ZoneUCSDUE(UINum).D_Kc = 0.0069;
                ZoneUCSDUE(UINum).E_Kc = -0.00004;
            } else if (ZoneUCSDUE(UINum).DiffuserType == VarArea) {
                if (ZoneUCSDUE(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUE(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionExterior for Zone " + ZoneUCSDUE(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = VariableArea.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUE(UINum).A_Kc = 0.0;
                ZoneUCSDUE(UINum).B_Kc = 0.0;
                ZoneUCSDUE(UINum).C_Kc = 0.83;
                ZoneUCSDUE(UINum).D_Kc = 0.0;
                ZoneUCSDUE(UINum).E_Kc = 0.0;
            } else if (ZoneUCSDUE(UINum).DiffuserType == DisplVent) {
                if (ZoneUCSDUE(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUE(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionExterior for Zone " + ZoneUCSDUE(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = HorizontalDisplacement.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUE(UINum).A_Kc = 0.0;
                ZoneUCSDUE(UINum).B_Kc = 0.0;
                ZoneUCSDUE(UINum).C_Kc = 0.67;
                ZoneUCSDUE(UINum).D_Kc = 0.0;
                ZoneUCSDUE(UINum).E_Kc = 0.0;
            } else if (ZoneUCSDUE(UINum).DiffuserType == LinBarGrille) {
                if (ZoneUCSDUE(UINum).A_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).B_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).C_Kc != DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUE(UINum).D_Kc != DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).E_Kc != DataGlobalConstants::AutoCalculate()) {
                    ShowWarningError(state, "For RoomAirSettings:UnderFloorAirDistributionExterior for Zone " + ZoneUCSDUE(UINum).ZoneName +
                                     ", input for Coefficients A - E will be ignored when Floor Diffuser Type = LinearBarGrille.");
                    ShowContinueError(state, "  To input these Coefficients, use Floor Diffuser Type = Custom.");
                }
                ZoneUCSDUE(UINum).A_Kc = 0.0;
                ZoneUCSDUE(UINum).B_Kc = 0.0;
                ZoneUCSDUE(UINum).C_Kc = 0.8214;
                ZoneUCSDUE(UINum).D_Kc = -0.0263;
                ZoneUCSDUE(UINum).E_Kc = 0.0014;
            } else {
                if (ZoneUCSDUE(UINum).A_Kc == DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).B_Kc == DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).C_Kc == DataGlobalConstants::AutoCalculate() ||
                    ZoneUCSDUE(UINum).D_Kc == DataGlobalConstants::AutoCalculate() || ZoneUCSDUE(UINum).E_Kc == DataGlobalConstants::AutoCalculate()) {
                    ShowFatalError(state, "For RoomAirSettings:UnderFloorAirDistributionExterior for Zone " + ZoneUCSDUE(UINum).ZoneName +
                                   ", input for Coefficients A - E must be specified when Floor Diffuser Type = Custom.");
                }
            }
            if (ZoneUCSDUE(UINum).PowerPerPlume == DataGlobalConstants::AutoCalculate()) {
                if (NumberOfOccupants > 0) {
                    NumberOfPlumes = NumberOfOccupants;
                } else {
                    NumberOfPlumes = 1.0;
                }
                ZoneElecConv = 0.0;
                for (Ctd = 1; Ctd <= TotElecEquip; ++Ctd) {
                    if (ZoneElectric(Ctd).ZonePtr == ZoneNum) {
                        ZoneElecConv += ZoneElectric(Ctd).DesignLevel;
                    }
                }
                ZoneGasConv = 0.0;
                for (Ctd = 1; Ctd <= TotGasEquip; ++Ctd) {
                    if (ZoneGas(Ctd).ZonePtr == ZoneNum) {
                        ZoneGasConv += ZoneGas(Ctd).DesignLevel;
                    }
                }
                ZoneOthEqConv = 0.0;
                for (Ctd = 1; Ctd <= TotOthEquip; ++Ctd) {
                    if (ZoneOtherEq(Ctd).ZonePtr == ZoneNum) {
                        ZoneOthEqConv += ZoneOtherEq(Ctd).DesignLevel;
                    }
                }
                ZoneHWEqConv = 0.0;
                for (Ctd = 1; Ctd <= TotHWEquip; ++Ctd) {
                    if (ZoneHWEq(Ctd).ZonePtr == ZoneNum) {
                        ZoneHWEqConv += ZoneHWEq(Ctd).DesignLevel;
                    }
                }
                for (Ctd = 1; Ctd <= TotStmEquip; ++Ctd) {
                    ZoneSteamEqConv = 0.0;
                    if (ZoneSteamEq(Ctd).ZonePtr == ZoneNum) {
                        ZoneSteamEqConv += ZoneSteamEq(Ctd).DesignLevel;
                    }
                }
                ZoneUCSDUE(UINum).PowerPerPlume =
                    (NumberOfOccupants * 73.0 + ZoneElecConv + ZoneGasConv + ZoneOthEqConv + ZoneHWEqConv + ZoneSteamEqConv) / NumberOfPlumes;
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionExterior",
                                             ZoneUCSDUE(UINum).ZoneName,
                                             "Power per plume [W]",
                                             ZoneUCSDUE(UINum).PowerPerPlume);
            }
            if (ZoneUCSDUE(UINum).DiffusersPerZone == AutoSize) {
                if (NumberOfOccupants > 0.0) {
                    ZoneUCSDUE(UINum).DiffusersPerZone = NumberOfOccupants;
                } else {
                    ZoneUCSDUE(UINum).DiffusersPerZone = 1.0;
                }
                BaseSizer::reportSizerOutput(state, "RoomAirSettings:UnderFloorAirDistributionExterior",
                                             ZoneUCSDUE(UINum).ZoneName,
                                             "Number of diffusers per zone",
                                             ZoneUCSDUE(UINum).DiffusersPerZone);
            }
        }
    }

    void HcUCSDUF(EnergyPlusData &state, int const ZoneNum, Real64 const FractionHeight)
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         G. Carrilho da Graca
        //       DATE WRITTEN   February 2004
        //       MODIFIED       -
        //       RE-ENGINEERED  -

        // PURPOSE OF THIS SUBROUTINE:
        // Main subroutine for convection calculation in the UCSD Displacement Ventilation model.
        // It calls CalcDetailedHcInForDVModel for convection coefficient
        // initial calculations and averages the final result comparing the position of the surface with
        // the interface subzone height.

        // Using/Aliasing
        using namespace DataHeatBalFanSys;
        using namespace DataEnvironment;
        using namespace DataHeatBalance;
        using ScheduleManager::GetScheduleIndex;

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        int Ctd;         // DO loop counter for surfaces
        Real64 HLD;      // Convection coefficient for the lower area of surface
        Real64 TmedDV;   // Average temperature for DV
        Real64 Z1;       // auxiliary var for lowest height
        Real64 Z2;       // auxiliary var for highest height
        Real64 ZSupSurf; // highest height for this surface
        Real64 ZInfSurf; // lowest height for this surface
        Real64 HLU;      // Convection coefficient for the upper area of surface
        Real64 LayH;     // Height of the Occupied/Mixed subzone interface
        Real64 LayFrac;  // Fraction height of the Occupied/Mixed subzone interface
        int SurfNum;     // Surface number
        // Initialize HAT and HA

        state.dataUFADManager->HAT_MX = 0.0;
        state.dataUFADManager->HAT_OC = 0.0;
        state.dataUFADManager->HA_MX = 0.0;
        state.dataUFADManager->HA_OC = 0.0;
        state.dataUFADManager->HAT_FLOOR = 0.0;
        state.dataUFADManager->HA_FLOOR = 0.0;
        state.dataUFADManager->HAT_MXWin = 0.0;
        state.dataUFADManager->HAT_OCWin = 0.0;
        state.dataUFADManager->HA_MXWin = 0.0;
        state.dataUFADManager->HA_OCWin = 0.0;

        // Is the air flow model for this zone set to UCSDDV Displacement Ventilation?
        if (IsZoneUI(ZoneNum)) {
            LayFrac = FractionHeight;
            LayH = FractionHeight * (ZoneCeilingHeight((ZoneNum - 1) * 2 + 2) - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1));
            // WALL Hc, HA and HAT calculation
            for (Ctd = PosZ_Wall((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Wall((ZoneNum - 1) * 2 + 2); ++Ctd) {
                SurfNum = APos_Wall(Ctd);
                Surface(SurfNum).TAirRef = AdjacentAirTemp;
                if (SurfNum == 0) continue;
                Z1 = minval(Surface(SurfNum).Vertex({1, Surface(SurfNum).Sides}), &Vector::z);
                Z2 = maxval(Surface(SurfNum).Vertex({1, Surface(SurfNum).Sides}), &Vector::z);
                ZSupSurf = Z2 - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);
                ZInfSurf = Z1 - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);

                // The Wall surface is in the upper subzone
                if (ZInfSurf > LayH) {
                    TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HWall(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWall(Ctd);
                    state.dataUFADManager->HA_MX += Surface(SurfNum).Area * HWall(Ctd);
                }

                // The Wall surface is in the lower subzone
                if (ZSupSurf < LayH) {
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HWall(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWall(Ctd);
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * HWall(Ctd);
                }

                if (std::abs(ZInfSurf - ZSupSurf) < 1.e-10) {
                    ShowSevereError(state, "RoomAirModelUFAD:HcUCSDUF: Surface values will cause divide by zero.");
                    ShowContinueError(state, "Zone=\"" + Zone(Surface(SurfNum).Zone).Name + "\", Surface=\"" + Surface(SurfNum).Name + "\".");
                    ShowContinueError(state, format("ZInfSurf=[{:.4R}], LayH=[{:.4R}].", ZInfSurf, LayH));
                    ShowContinueError(state, format("ZSupSurf=[{:.4R}], LayH=[{:.4R}].", ZSupSurf, LayH));
                    ShowFatalError(state, "...Previous condition causes termination.");
                }

                // The Wall surface is partially in upper and partially in lower subzone
                if (ZInfSurf <= LayH && ZSupSurf >= LayH) {
                    TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HLU = UFHcIn(SurfNum);
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HLD = UFHcIn(SurfNum);
                    TmedDV = ((ZSupSurf - LayH) * ZTMX(ZoneNum) + (LayH - ZInfSurf) * ZTOC(ZoneNum)) / (ZSupSurf - ZInfSurf);
                    HWall(Ctd) = ((LayH - ZInfSurf) * HLD + (ZSupSurf - LayH) * HLU) / (ZSupSurf - ZInfSurf);
                    state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLU;
                    state.dataUFADManager->HA_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * HLU;
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLD;
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * HLD;
                    TempEffBulkAir(SurfNum) = TmedDV;
                }

                UFHcIn(SurfNum) = HWall(Ctd);

            } // END WALL

            // WINDOW Hc, HA and HAT CALCULATION
            for (Ctd = PosZ_Window((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Window((ZoneNum - 1) * 2 + 2); ++Ctd) {
                SurfNum = APos_Window(Ctd);
                Surface(SurfNum).TAirRef = AdjacentAirTemp;
                if (SurfNum == 0) continue;
                if (Surface(SurfNum).Tilt > 10.0 && Surface(SurfNum).Tilt < 170.0) { // Window Wall
                    Z1 = minval(Surface(SurfNum).Vertex({1, Surface(SurfNum).Sides}), &Vector::z);
                    Z2 = maxval(Surface(SurfNum).Vertex({1, Surface(SurfNum).Sides}), &Vector::z);
                    ZSupSurf = Z2 - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);
                    ZInfSurf = Z1 - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);

                    if (ZInfSurf > LayH) {
                        TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                        CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                        HWindow(Ctd) = UFHcIn(SurfNum);
                        state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWindow(Ctd);
                        state.dataUFADManager->HA_MX += Surface(SurfNum).Area * HWindow(Ctd);
                        state.dataUFADManager->HAT_MXWin += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWindow(Ctd);
                        state.dataUFADManager->HA_MXWin += Surface(SurfNum).Area * HWindow(Ctd);
                    }

                    if (ZSupSurf < LayH) {
                        TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                        CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                        HWindow(Ctd) = UFHcIn(SurfNum);
                        state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWindow(Ctd);
                        state.dataUFADManager->HA_OC += Surface(SurfNum).Area * HWindow(Ctd);
                        state.dataUFADManager->HAT_OCWin += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWindow(Ctd);
                        state.dataUFADManager->HA_OCWin += Surface(SurfNum).Area * HWindow(Ctd);
                    }

                    if (ZInfSurf <= LayH && ZSupSurf >= LayH) {
                        TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                        CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                        HLU = UFHcIn(SurfNum);
                        TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                        CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                        HLD = UFHcIn(SurfNum);
                        TmedDV = ((ZSupSurf - LayH) * ZTMX(ZoneNum) + (LayH - ZInfSurf) * ZTOC(ZoneNum)) / (ZSupSurf - ZInfSurf);
                        HWindow(Ctd) = ((LayH - ZInfSurf) * HLD + (ZSupSurf - LayH) * HLU) / (ZSupSurf - ZInfSurf);
                        state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLU;
                        state.dataUFADManager->HA_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * HLU;
                        state.dataUFADManager->HAT_MXWin += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLU;
                        state.dataUFADManager->HA_MXWin += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * HLU;
                        state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLD;
                        state.dataUFADManager->HA_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * HLD;
                        state.dataUFADManager->HAT_OCWin += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLD;
                        state.dataUFADManager->HA_OCWin += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * HLD;
                        TempEffBulkAir(SurfNum) = TmedDV;
                    }
                }

                if (Surface(SurfNum).Tilt <= 10.0) { // Window Ceiling
                    TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HWindow(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWindow(Ctd);
                    state.dataUFADManager->HA_MX += Surface(SurfNum).Area * HWindow(Ctd);
                }

                if (Surface(SurfNum).Tilt >= 170.0) { // Window Floor
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HWindow(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HWindow(Ctd);
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * HWindow(Ctd);
                }

                UFHcIn(SurfNum) = HWindow(Ctd);

            } // END WINDOW

            // DOOR Hc, HA and HAT CALCULATION
            for (Ctd = PosZ_Door((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Door((ZoneNum - 1) * 2 + 2); ++Ctd) { // DOOR
                SurfNum = APos_Door(Ctd);
                Surface(SurfNum).TAirRef = AdjacentAirTemp;
                if (SurfNum == 0) continue;
                Z1 = minval(Surface(SurfNum).Vertex({1, Surface(SurfNum).Sides}), &Vector::z);
                Z2 = maxval(Surface(SurfNum).Vertex({1, Surface(SurfNum).Sides}), &Vector::z);
                ZSupSurf = Z2 - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);
                ZInfSurf = Z1 - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);

                if (ZInfSurf > LayH) {
                    TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HDoor(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HDoor(Ctd);
                    state.dataUFADManager->HA_MX += Surface(SurfNum).Area * HDoor(Ctd);
                }

                if (ZSupSurf < LayH) {
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HDoor(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HDoor(Ctd);
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * HDoor(Ctd);
                }

                if (ZInfSurf <= LayH && ZSupSurf >= LayH) {
                    TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HLU = UFHcIn(SurfNum);
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HLD = UFHcIn(SurfNum);
                    TmedDV = ((ZSupSurf - LayH) * ZTMX(ZoneNum) + (LayH - ZInfSurf) * ZTOC(ZoneNum)) / (ZSupSurf - ZInfSurf);
                    HDoor(Ctd) = ((LayH - ZInfSurf) * HLD + (ZSupSurf - LayH) * HLU) / (ZSupSurf - ZInfSurf);
                    state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLU;
                    state.dataUFADManager->HA_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * HLU;
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLD;
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * HLD;
                    TempEffBulkAir(SurfNum) = TmedDV;
                }

                UFHcIn(SurfNum) = HDoor(Ctd);

            } // END DOOR

            // INTERNAL Hc, HA and HAT CALCULATION
            state.dataUFADManager->HeightIntMass = min(state.dataUFADManager->HeightIntMassDefault, (ZoneCeilingHeight((ZoneNum - 1) * 2 + 2) - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1)));
            for (Ctd = PosZ_Internal((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Internal((ZoneNum - 1) * 2 + 2); ++Ctd) {
                SurfNum = APos_Internal(Ctd);
                Surface(SurfNum).TAirRef = AdjacentAirTemp;
                if (SurfNum == 0) continue;
                ZSupSurf = state.dataUFADManager->HeightIntMass;
                ZInfSurf = 0.0;

                if (ZSupSurf < LayH) {
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HInternal(Ctd) = UFHcIn(SurfNum);
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HInternal(Ctd);
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * HInternal(Ctd);
                }

                if (ZInfSurf <= LayH && ZSupSurf >= LayH) {
                    TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HLU = UFHcIn(SurfNum);
                    TempEffBulkAir(SurfNum) = ZTOC(ZoneNum);
                    CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                    HLD = UFHcIn(SurfNum);
                    TmedDV = ((ZSupSurf - LayH) * ZTMX(ZoneNum) + (LayH - ZInfSurf) * ZTOC(ZoneNum)) / (ZSupSurf - ZInfSurf);
                    HInternal(Ctd) = ((LayH - ZInfSurf) * HLD + (ZSupSurf - LayH) * HLU) / (ZSupSurf - ZInfSurf);
                    state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLU;
                    state.dataUFADManager->HA_MX += Surface(SurfNum).Area * (ZSupSurf - LayH) / (ZSupSurf - ZInfSurf) * HLU;
                    state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * TempSurfIn(SurfNum) * HLD;
                    state.dataUFADManager->HA_OC += Surface(SurfNum).Area * (LayH - ZInfSurf) / (ZSupSurf - ZInfSurf) * HLD;
                    TempEffBulkAir(SurfNum) = TmedDV;
                }

                UFHcIn(SurfNum) = HInternal(Ctd);
            } // END INTERNAL

            // CEILING Hc, HA and HAT CALCULATION
            for (Ctd = PosZ_Ceiling((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Ceiling((ZoneNum - 1) * 2 + 2); ++Ctd) {
                SurfNum = APos_Ceiling(Ctd);
                Surface(SurfNum).TAirRef = AdjacentAirTemp;
                if (SurfNum == 0) continue;
                TempEffBulkAir(SurfNum) = ZTMX(ZoneNum);
                CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                HCeiling(Ctd) = UFHcIn(SurfNum);
                state.dataUFADManager->HAT_MX += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HCeiling(Ctd);
                state.dataUFADManager->HA_MX += Surface(SurfNum).Area * HCeiling(Ctd);
                UFHcIn(SurfNum) = HCeiling(Ctd);
            } // END CEILING

            // FLOOR Hc, HA and HAT CALCULATION
            for (Ctd = PosZ_Floor((ZoneNum - 1) * 2 + 1); Ctd <= PosZ_Floor((ZoneNum - 1) * 2 + 2); ++Ctd) {
                SurfNum = APos_Floor(Ctd);
                Surface(SurfNum).TAirRef = AdjacentAirTemp;
                if (SurfNum == 0) continue;
                TempEffBulkAir(SurfNum) = ZTFloor(ZoneNum);
                CalcDetailedHcInForDVModel(state, SurfNum, TempSurfIn, UFHcIn);
                HFloor(Ctd) = UFHcIn(SurfNum);
                state.dataUFADManager->HAT_OC += Surface(SurfNum).Area * TempSurfIn(SurfNum) * HFloor(Ctd);
                state.dataUFADManager->HA_OC += Surface(SurfNum).Area * HFloor(Ctd);
                TempEffBulkAir(SurfNum) = ZTFloor(ZoneNum);
                UFHcIn(SurfNum) = HFloor(Ctd);
            } // END FLOOR
        }
    }

    void CalcUCSDUI(EnergyPlusData &state, int const ZoneNum) // index number for the specified zone
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   August 2005
        //       MODIFIED       Brent Griffith June 2008 for new interpolation and time history
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Using the UCSD UFAD interior zone model, this subroutine calculates the  occupied subzone height,
        // surface heat transfer coefficients, the occupied subzone temperature, and the upper subzone temperature.

        // METHODOLOGY EMPLOYED:
        // The zone is divided into 2 subzones with a variable transition height.

        // REFERENCES:
        // The model is described in the EnergyPlus Engineering Reference in Anna Liu's UCSD PhD thesis.

        // Using/Aliasing
        using DataZoneEquipment::ZoneEquipConfig;
        using Psychrometrics::PsyCpAirFnW;
        using Psychrometrics::PsyRhoAirFnPbTdbW;
        using ScheduleManager::GetCurrentScheduleValue;
        using namespace DataHeatBalFanSys;
        using DataHVACGlobals::TimeStepSys;
        using DataHVACGlobals::UseZoneTimeStepHistory;
        using InternalHeatGains::SumInternalConvectionGainsByTypes;
        using InternalHeatGains::SumReturnAirConvectionGainsByTypes;

        // Locals
        // SUBROUTINE ARGUMENT DEFINITIONS:

        // SUBROUTINE PARAMETER DEFINITIONS:
        // na

        // INTERFACE BLOCK SPECIFICATIONS:
        // na

        // DERIVED TYPE DEFINITIONS:
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        static bool MIXFLAG(false); // if true treat as a mixed zone
        Real64 CeilingHeight;       // zone ceiling height above floor [m]
        int UINum;                  // index to underfloor interior zone model data
        Real64 GainsFrac;           // fraction of occupied subzone heat gains that remain in the subzone;
        // that is, don't go into the plumes
        // REAL(r64)   :: NumPLPP            ! number of plumes per person
        Real64 HeightThermostat;    // height of the thermostat above the floor [m]
        Real64 HeightComfort;       // height at which comfort temperature is calculated
        Real64 TempDiffCritRep;     // Minimum temperature difference between upper and occupied subzones for reporting
        Real64 ConvGainsOccSubzone; // convective heat gains into the lower (occupied) subzone [W]
        Real64 ConvGainsUpSubzone;  // convective heat gains into the upper subzone [W]
        Real64 ConvGains;           // total zone convective gains (exclusing surfaces) [W]
        int ZoneEquipConfigNum;     // ZoneEquipConfig index for this UFAD zone
        Real64 SumSysMCp;           // Sum of system mass flow rate * specific heat for this zone [W/K]
        Real64 SumSysMCpT;          // Sum of system mass flow rate * specific heat * temperature for this zone [W]
        Real64 SumSysM;             // Sum of systems mass flow rate [kg/s]
        Real64 NodeTemp;            // inlet node temperature [K]
        Real64 MassFlowRate;        // system mass flow rate [kg/s]
        Real64 CpAir;               // specific heat of air [J/kgK]
        int InNodeIndex;            // inlet node index in ZoneEquipConfig
        Real64 SumMCp;              // mass flow rate * specific heat for this zone for infiltration, ventilation, mixing [W/K]
        Real64 SumMCpT;             // mass flow rate * specific heat* temp for this zone for infiltration, ventilation, mixing [W]
        Real64 MCp_Total;           // total mass flow rate * specific heat for this zone [W/K]
        Real64 MCpT_Total;          // total mass flow rate * specific heat* temp for this zone [W]
        Real64 NumberOfPlumes;
        Real64 PowerInPlumes;             // [W]
        static Real64 PowerPerPlume(0.0); // power generating each plume [W]
        Real64 HeightFrac;                // Fractional height of transition between occupied and upper subzones
        Real64 TotSysFlow;                // [m3/s]
        Real64 NumDiffusersPerPlume;
        Real64 NumDiffusers;
        Real64 TSupK; // supply yemperature [K]
        Real64 Gamma; // dimensionless height parameter; higher gamma means interface height will be
        // higher, smaller gamma means interface height will be lower.
        Real64 DiffArea;     // diffuser effective area [m2]
        Real64 ThrowAngle;   // diffuser slot angle relative to vertical [radians]
        Real64 SourceHeight; // height of plume sources above the floor [m]
        int Ctd;
        Real64 AirCap;
        Real64 TempHistTerm;
        Real64 ZTAveraged;
        Real64 HeightUpSubzoneAve;       // Height of center of upper air subzone
        Real64 HeightOccupiedSubzoneAve; // Height of center of occupied air subzone
        Real64 ZoneMult;                 // total zone multiplier
        int ZoneNodeNum;                 // node number of the HVAC zone node
        static Real64 TempDepCoef(0.0);  // Formerly CoefSumha, coef in zone temp equation with dimensions of h*A
        static Real64 TempIndCoef(0.0);  // Formerly CoefSumhat, coef in zone temp equation with dimensions of h*A(T1
        static Array1D_int IntGainTypesOccupied(29,
                                                {IntGainTypeOf_People,
                                                 IntGainTypeOf_WaterHeaterMixed,
                                                 IntGainTypeOf_WaterHeaterStratified,
                                                 IntGainTypeOf_ThermalStorageChilledWaterMixed,
                                                 IntGainTypeOf_ThermalStorageChilledWaterStratified,
                                                 IntGainTypeOf_ElectricEquipment,
                                                 IntGainTypeOf_ElectricEquipmentITEAirCooled,
                                                 IntGainTypeOf_GasEquipment,
                                                 IntGainTypeOf_HotWaterEquipment,
                                                 IntGainTypeOf_SteamEquipment,
                                                 IntGainTypeOf_OtherEquipment,
                                                 IntGainTypeOf_ZoneBaseboardOutdoorTemperatureControlled,
                                                 IntGainTypeOf_GeneratorFuelCell,
                                                 IntGainTypeOf_WaterUseEquipment,
                                                 IntGainTypeOf_GeneratorMicroCHP,
                                                 IntGainTypeOf_ElectricLoadCenterTransformer,
                                                 IntGainTypeOf_ElectricLoadCenterInverterSimple,
                                                 IntGainTypeOf_ElectricLoadCenterInverterFunctionOfPower,
                                                 IntGainTypeOf_ElectricLoadCenterInverterLookUpTable,
                                                 IntGainTypeOf_ElectricLoadCenterStorageBattery,
                                                 IntGainTypeOf_ElectricLoadCenterStorageSimple,
                                                 IntGainTypeOf_PipeIndoor,
                                                 IntGainTypeOf_RefrigerationCase,
                                                 IntGainTypeOf_RefrigerationCompressorRack,
                                                 IntGainTypeOf_RefrigerationSystemAirCooledCondenser,
                                                 IntGainTypeOf_RefrigerationSystemSuctionPipe,
                                                 IntGainTypeOf_RefrigerationSecondaryReceiver,
                                                 IntGainTypeOf_RefrigerationSecondaryPipe,
                                                 IntGainTypeOf_RefrigerationWalkIn});

        static Array1D_int IntGainTypesUpSubzone(2, {IntGainTypeOf_DaylightingDeviceTubular, IntGainTypeOf_Lights});
        Real64 RetAirGains;

        // Exact solution or Euler method
        if (ZoneAirSolutionAlgo != Use3rdOrder) {
            if (ShortenTimeStepSysRoomAir && TimeStepSys < state.dataGlobal->TimeStepZone) {
                if (PreviousTimeStep < state.dataGlobal->TimeStepZone) {
                    Zone1OC(ZoneNum) = ZoneM2OC(ZoneNum);
                    Zone1MX(ZoneNum) = ZoneM2MX(ZoneNum);
                } else {
                    Zone1OC(ZoneNum) = ZoneMXOC(ZoneNum);
                    Zone1MX(ZoneNum) = ZoneMXMX(ZoneNum);
                }
            } else {
                Zone1OC(ZoneNum) = ZTOC(ZoneNum);
                Zone1MX(ZoneNum) = ZTMX(ZoneNum);
            }
        }

        MIXFLAG = false;
        UFHcIn = HConvIn;
        SumSysMCp = 0.0;
        SumSysMCpT = 0.0;
        TotSysFlow = 0.0;
        TSupK = 0.0;
        SumSysM = 0.0;
        ZoneMult = Zone(ZoneNum).Multiplier * Zone(ZoneNum).ListMultiplier;
        CeilingHeight = ZoneCeilingHeight((ZoneNum - 1) * 2 + 2) - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);
        UINum = ZoneUFPtr(ZoneNum);
        HeightThermostat = ZoneUCSDUI(UINum).ThermostatHeight;
        HeightComfort = ZoneUCSDUI(UINum).ComfortHeight;
        TempDiffCritRep = ZoneUCSDUI(UINum).TempTrigger;
        DiffArea = ZoneUCSDUI(UINum).DiffArea;
        ThrowAngle = DataGlobalConstants::DegToRadians() * ZoneUCSDUI(UINum).DiffAngle;
        SourceHeight = 0.0;
        NumDiffusers = ZoneUCSDUI(UINum).DiffusersPerZone;
        PowerPerPlume = ZoneUCSDUI(UINum).PowerPerPlume;
        // gains from occupants, task lighting, elec equip, gas equip, other equip, hot water equip, steam equip,
        // baseboards (nonthermostatic), water heater skin loss
        SumInternalConvectionGainsByTypes(ZoneNum, IntGainTypesOccupied, ConvGainsOccSubzone);

        // Add heat to return air if zonal system (no return air) or cycling system (return air frequently very
        // low or zero)
        if (Zone(ZoneNum).NoHeatToReturnAir) {
            SumReturnAirConvectionGainsByTypes(ZoneNum, IntGainTypesOccupied, RetAirGains);
            ConvGainsOccSubzone += RetAirGains;
        }

        // Add convection from pool cover to occupied region
        ConvGainsOccSubzone += SumConvPool(ZoneNum);

        // gains from lights (ceiling), tubular daylighting devices, high temp radiant heaters

        SumInternalConvectionGainsByTypes(ZoneNum, IntGainTypesUpSubzone, ConvGainsUpSubzone);
        ConvGainsUpSubzone += SumConvHTRadSys(ZoneNum);
        if (Zone(ZoneNum).NoHeatToReturnAir) {
            SumReturnAirConvectionGainsByTypes(ZoneNum, IntGainTypesUpSubzone, RetAirGains);
            ConvGainsUpSubzone += RetAirGains;
        }
        ConvGains = ConvGainsOccSubzone + ConvGainsUpSubzone + SysDepZoneLoadsLagged(ZoneNum);
        ZoneEquipConfigNum = ZoneUCSDUI(UINum).ZoneEquipPtr;
        if (ZoneEquipConfigNum > 0) {
            for (InNodeIndex = 1; InNodeIndex <= ZoneEquipConfig(ZoneEquipConfigNum).NumInletNodes; ++InNodeIndex) {
                NodeTemp = Node(ZoneEquipConfig(ZoneEquipConfigNum).InletNode(InNodeIndex)).Temp;
                MassFlowRate = Node(ZoneEquipConfig(ZoneEquipConfigNum).InletNode(InNodeIndex)).MassFlowRate;
                CpAir = PsyCpAirFnW(ZoneAirHumRat(ZoneNum));
                SumSysMCp += MassFlowRate * CpAir;
                SumSysMCpT += MassFlowRate * CpAir * NodeTemp;
                TotSysFlow += MassFlowRate / PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, NodeTemp, ZoneAirHumRat(ZoneNum));
                TSupK += MassFlowRate * NodeTemp;
                SumSysM += MassFlowRate;
            }
            if (TotSysFlow > 0.0) {
                TSupK = TSupK / SumSysM + DataGlobalConstants::KelvinConv();
            } else {
                TSupK = 0.0;
            }
        }
        // mass flow times specific heat for infiltration, ventilation, mixing, earth tube
        SumMCp = MCPI(ZoneNum) + MCPV(ZoneNum) + MCPM(ZoneNum) + MCPE(ZoneNum) + MCPC(ZoneNum) + MDotCPOA(ZoneNum);
        // mass flow times specific heat times temperature for infiltration, ventilation, mixing, earth tube
        SumMCpT =
            MCPTI(ZoneNum) + MCPTV(ZoneNum) + MCPTM(ZoneNum) + MCPTE(ZoneNum) + MCPTC(ZoneNum) + MDotCPOA(ZoneNum) * Zone(ZoneNum).OutDryBulbTemp;
        MCp_Total = SumMCp + SumSysMCp;
        MCpT_Total = SumMCpT + SumSysMCpT;
        // For the York MIT diffusers (variable area) the area varies with the flow rate. Assume 400 ft/min velocity
        // at the diffuser, and a design flow rate of 150 cfm (.0708 m3/s). Then the design area for each diffuser is
        // 150 ft3/min / 400 ft/min = .375 ft2 = .035 m2. This is adjusted each time step by
        //               (TotSysFlow/(NumDiffusers*.0708))*.035
        if (ZoneUCSDUI(UINum).DiffuserType == VarArea) {
            DiffArea = 0.035 * TotSysFlow / (0.0708 * NumDiffusers);
        }
        // initial estimate of convective transfer from surfaces; assume HeightFrac is 0.5.
        HcUCSDUF(state, ZoneNum, 0.5);
        PowerInPlumes = ConvGains + state.dataUFADManager->HAT_OC - state.dataUFADManager->HA_OC * ZTOC(ZoneNum) + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum);
        if (PowerPerPlume > 0.0 && PowerInPlumes > 0.0) {
            NumberOfPlumes = PowerInPlumes / PowerPerPlume;
            NumDiffusersPerPlume = NumDiffusers / NumberOfPlumes;
        } else {
            NumberOfPlumes = 1.0;
            NumDiffusersPerPlume = 1.0;
        }
        if ((PowerInPlumes <= 0.0) || (TotSysFlow == 0.0) || (TSupK - DataGlobalConstants::KelvinConv()) > MAT(ZoneNum)) {
            // The system will mix
            HeightFrac = 0.0;
        } else {
            Gamma = std::pow(TotSysFlow * std::cos(ThrowAngle), 1.5) /
                    (NumberOfPlumes * std::pow(NumDiffusersPerPlume * DiffArea, 1.25) * std::sqrt(0.0281 * 0.001 * PowerInPlumes));
            if (ZoneUCSDUI(UINum).CalcTransHeight) {
                HeightFrac = (std::sqrt(NumDiffusersPerPlume * DiffArea) * (7.43 * std::log(Gamma) - 1.35) + 0.5 * SourceHeight) / CeilingHeight;
            } else {
                HeightFrac = ZoneUCSDUI(UINum).TransHeight / CeilingHeight;
            }
            HeightFrac = max(0.0, min(1.0, HeightFrac));
            for (Ctd = 1; Ctd <= 4; ++Ctd) {
                HcUCSDUF(state, ZoneNum, HeightFrac);
                PowerInPlumes = ConvGains + state.dataUFADManager->HAT_OC - state.dataUFADManager->HA_OC * ZTOC(ZoneNum) + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum);
                if (PowerPerPlume > 0.0 && PowerInPlumes > 0.0) {
                    NumberOfPlumes = PowerInPlumes / PowerPerPlume;
                    NumDiffusersPerPlume = NumDiffusers / NumberOfPlumes;
                } else {
                    NumberOfPlumes = 1.0;
                    NumDiffusersPerPlume = 1.0;
                }
                if (PowerInPlumes <= 0.0) break;
                Gamma = std::pow(TotSysFlow * std::cos(ThrowAngle), 1.5) /
                        (NumberOfPlumes * std::pow(NumDiffusersPerPlume * DiffArea, 1.25) * std::sqrt(0.0281 * 0.001 * PowerInPlumes));
                if (ZoneUCSDUI(UINum).CalcTransHeight) {
                    HeightFrac = (std::sqrt(NumDiffusersPerPlume * DiffArea) * (7.43 * std::log(Gamma) - 1.35) + 0.5 * SourceHeight) / CeilingHeight;
                } else {
                    HeightFrac = ZoneUCSDUI(UINum).TransHeight / CeilingHeight;
                }
                HeightFrac = max(0.0, min(1.0, HeightFrac));
                HeightTransition(ZoneNum) = HeightFrac * CeilingHeight;
                GainsFrac = ZoneUCSDUI(UINum).A_Kc * std::pow(Gamma, ZoneUCSDUI(UINum).B_Kc) + ZoneUCSDUI(UINum).C_Kc +
                            ZoneUCSDUI(UINum).D_Kc * Gamma + ZoneUCSDUI(UINum).E_Kc * pow_2(Gamma);
                GainsFrac = max(0.6, min(GainsFrac, 1.0));
                AIRRATOC(ZoneNum) = Zone(ZoneNum).Volume * (HeightTransition(ZoneNum) - min(HeightTransition(ZoneNum), 0.2)) / CeilingHeight *
                                    Zone(ZoneNum).ZoneVolCapMultpSens * PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, MATOC(ZoneNum), ZoneAirHumRat(ZoneNum)) *
                                    PsyCpAirFnW(ZoneAirHumRat(ZoneNum)) / (TimeStepSys * DataGlobalConstants::SecInHour());
                AIRRATMX(ZoneNum) = Zone(ZoneNum).Volume * (CeilingHeight - HeightTransition(ZoneNum)) / CeilingHeight *
                                    Zone(ZoneNum).ZoneVolCapMultpSens * PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, MATMX(ZoneNum), ZoneAirHumRat(ZoneNum)) *
                                    PsyCpAirFnW(ZoneAirHumRat(ZoneNum)) / (TimeStepSys * DataGlobalConstants::SecInHour());

                if (UseZoneTimeStepHistory) {
                    ZTM3OC(ZoneNum) = XM3TOC(ZoneNum);
                    ZTM2OC(ZoneNum) = XM2TOC(ZoneNum);
                    ZTM1OC(ZoneNum) = XMATOC(ZoneNum);

                    ZTM3MX(ZoneNum) = XM3TMX(ZoneNum);
                    ZTM2MX(ZoneNum) = XM2TMX(ZoneNum);
                    ZTM1MX(ZoneNum) = XMATMX(ZoneNum);

                } else {
                    ZTM3OC(ZoneNum) = DSXM3TOC(ZoneNum);
                    ZTM2OC(ZoneNum) = DSXM2TOC(ZoneNum);
                    ZTM1OC(ZoneNum) = DSXMATOC(ZoneNum);

                    ZTM3MX(ZoneNum) = DSXM3TMX(ZoneNum);
                    ZTM2MX(ZoneNum) = DSXM2TMX(ZoneNum);
                    ZTM1MX(ZoneNum) = DSXMATMX(ZoneNum);
                }

                AirCap = AIRRATOC(ZoneNum);
                TempHistTerm = AirCap * (3.0 * ZTM1OC(ZoneNum) - (3.0 / 2.0) * ZTM2OC(ZoneNum) + (1.0 / 3.0) * ZTM3OC(ZoneNum));
                TempDepCoef = GainsFrac * state.dataUFADManager->HA_OC + MCp_Total;
                TempIndCoef =
                    GainsFrac * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum)) + MCpT_Total + NonAirSystemResponse(ZoneNum) / ZoneMult;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTOC(ZoneNum) = (TempHistTerm + GainsFrac * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum)) + MCpT_Total +
                                         NonAirSystemResponse(ZoneNum) / ZoneMult) /
                                        ((11.0 / 6.0) * AirCap + GainsFrac * state.dataUFADManager->HA_OC + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTOC(ZoneNum) = Zone1OC(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTOC(ZoneNum) = (Zone1OC(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                            TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTOC(ZoneNum) = (AirCap * Zone1OC(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                AirCap = AIRRATMX(ZoneNum);
                TempHistTerm = AirCap * (3.0 * ZTM1MX(ZoneNum) - (3.0 / 2.0) * ZTM2MX(ZoneNum) + (1.0 / 3.0) * ZTM3MX(ZoneNum));
                TempDepCoef = (1.0 - GainsFrac) * state.dataUFADManager->HA_MX + MCp_Total;
                TempIndCoef = (1.0 - GainsFrac) * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_OC * ZTOC(ZoneNum)) + ZTOC(ZoneNum) * MCp_Total;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTMX(ZoneNum) =
                            (TempHistTerm + (1.0 - GainsFrac) * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_OC * ZTOC(ZoneNum)) + ZTOC(ZoneNum) * MCp_Total) /
                            ((11.0 / 6.0) * AirCap + (1.0 - GainsFrac) * state.dataUFADManager->HA_MX + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTMX(ZoneNum) = Zone1MX(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTMX(ZoneNum) = (Zone1MX(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                            TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTMX(ZoneNum) = (AirCap * Zone1MX(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                ZTFloor(ZoneNum) = ZTOC(ZoneNum);
            }
            if (PowerInPlumes <= 0.0) {
                HeightFrac = 0.0;
                AirModel(ZoneNum).SimAirModel = false;
                ZoneUFGamma(ZoneNum) = 0.0;
                ZoneUFPowInPlumes(ZoneNum) = 0.0;
            } else {
                AirModel(ZoneNum).SimAirModel = true;
                ZoneUFGamma(ZoneNum) = Gamma;
                ZoneUFPowInPlumes(ZoneNum) = PowerInPlumes;
            }
        }

        //=============================== M I X E D  Calculation ==============================================
        if (ZTMX(ZoneNum) < ZTOC(ZoneNum) || MCp_Total <= 0.0 || HeightFrac * CeilingHeight < state.dataUFADManager->ThickOccupiedSubzoneMin) {
            MIXFLAG = true;
            HeightFrac = 0.0;
            AvgTempGrad(ZoneNum) = 0.0;
            MaxTempGrad(ZoneNum) = 0.0;
            AirModel(ZoneNum).SimAirModel = false;
            AirCap = AIRRAT(ZoneNum);
            TempHistTerm = AirCap * (3.0 * ZTM1(ZoneNum) - (3.0 / 2.0) * ZTM2(ZoneNum) + (1.0 / 3.0) * ZTM3(ZoneNum));

            for (Ctd = 1; Ctd <= 3; ++Ctd) {
                TempDepCoef = state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total;
                TempIndCoef = ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTAveraged = (TempHistTerm + ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total) / ((11.0 / 6.0) * AirCap + state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTAveraged = ZoneT1(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTAveraged = (ZoneT1(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                         TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTAveraged = (AirCap * ZoneT1(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                ZTOC(ZoneNum) = ZTAveraged;
                ZTMX(ZoneNum) = ZTAveraged;
                ZTFloor(ZoneNum) = ZTAveraged;
                HcUCSDUF(state, ZoneNum, HeightFrac);
                TempDepCoef = state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total;
                TempIndCoef = ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTAveraged = (TempHistTerm + ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total) / ((11.0 / 6.0) * AirCap + state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTAveraged = ZoneT1(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTAveraged = (ZoneT1(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                         TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTAveraged = (AirCap * ZoneT1(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                ZTOC(ZoneNum) = ZTAveraged;
                ZTMX(ZoneNum) = ZTAveraged;
                ZTFloor(ZoneNum) = ZTAveraged;
            }
        }
        //=========================================================================================

        // Comfort temperature and temperature at the thermostat/temperature control sensor

        HeightTransition(ZoneNum) = HeightFrac * CeilingHeight;
        HeightUpSubzoneAve = (CeilingHeight + HeightTransition(ZoneNum)) / 2.0;
        HeightOccupiedSubzoneAve = HeightTransition(ZoneNum) / 2.0;
        // Comfort temperature

        if (MIXFLAG) {
            TCMF(ZoneNum) = ZTAveraged;
        } else {
            if (HeightComfort < HeightOccupiedSubzoneAve) {
                TCMF(ZoneNum) = ZTOC(ZoneNum);
            } else if (HeightComfort >= HeightOccupiedSubzoneAve && HeightComfort < HeightUpSubzoneAve) {
                TCMF(ZoneNum) = (ZTOC(ZoneNum) * (HeightUpSubzoneAve - HeightComfort) + ZTMX(ZoneNum) * (HeightComfort - HeightOccupiedSubzoneAve)) /
                                (HeightUpSubzoneAve - HeightOccupiedSubzoneAve);
            } else if (HeightComfort >= HeightUpSubzoneAve && HeightComfort <= CeilingHeight) {
                TCMF(ZoneNum) = ZTMX(ZoneNum);
            } else {
                ShowFatalError(state, "UFAD comfort height is above ceiling or below floor in Zone: " + Zone(ZoneNum).Name);
            }
        }

        // Temperature at the thermostat/temperature control sensor

        if (MIXFLAG) {
            TempTstatAir(ZoneNum) = ZTAveraged;
        } else {
            if (HeightThermostat < HeightOccupiedSubzoneAve) {
                TempTstatAir(ZoneNum) = ZTOC(ZoneNum);
            } else if (HeightThermostat >= HeightOccupiedSubzoneAve && HeightThermostat < HeightUpSubzoneAve) {
                TempTstatAir(ZoneNum) =
                    (ZTOC(ZoneNum) * (HeightUpSubzoneAve - HeightThermostat) + ZTMX(ZoneNum) * (HeightThermostat - HeightOccupiedSubzoneAve)) /
                    (HeightUpSubzoneAve - HeightOccupiedSubzoneAve);
            } else if (HeightThermostat >= HeightUpSubzoneAve && HeightThermostat <= CeilingHeight) {
                TempTstatAir(ZoneNum) = ZTMX(ZoneNum);
            } else {
                ShowFatalError(state, "Underfloor air distribution thermostat height is above ceiling or below floor in Zone: " + Zone(ZoneNum).Name);
            }
        }

        // Temperature gradients
        if ((HeightUpSubzoneAve - HeightOccupiedSubzoneAve) > 0.1) {
            AvgTempGrad(ZoneNum) = (ZTMX(ZoneNum) - ZTOC(ZoneNum)) / (HeightUpSubzoneAve - HeightOccupiedSubzoneAve);
        } else {
            AvgTempGrad(ZoneNum) = 0.0;
        }

        if (MIXFLAG) {
            ZoneUFMixedFlag(ZoneNum) = 1;
            AirModel(ZoneNum).SimAirModel = false;
        } else {
            ZoneUFMixedFlag(ZoneNum) = 0;
            AirModel(ZoneNum).SimAirModel = true;
        }

        if (ZoneEquipConfigNum > 0) {
            ZoneNodeNum = Zone(ZoneNum).SystemZoneNodeNumber;
            Node(ZoneNodeNum).Temp = ZTMX(ZoneNum);
        }

        if (MIXFLAG) {
            Phi(ZoneNum) = 1.0;
        } else {
            Phi(ZoneNum) = (ZTOC(ZoneNum) - (TSupK - DataGlobalConstants::KelvinConv())) / (ZTMX(ZoneNum) - (TSupK - DataGlobalConstants::KelvinConv()));
        }

        // Mixed for reporting purposes
        if ((MIXFLAG) || ((ZTMX(ZoneNum) - ZTOC(ZoneNum)) < TempDiffCritRep)) {
            ZoneUFMixedFlagRep(ZoneNum) = 1.0;
            HeightTransition(ZoneNum) = 0.0;
            AvgTempGrad(ZoneNum) = 0.0;
        } else {
            ZoneUFMixedFlagRep(ZoneNum) = 0.0;
        }
    }

    void CalcUCSDUE(EnergyPlusData &state, int const ZoneNum) // index number for the specified zone
    {

        // SUBROUTINE INFORMATION:
        //       AUTHOR         Fred Buhl
        //       DATE WRITTEN   January 2006
        //       MODIFIED       Brent Griffith June 2008 for new interpolation and time history
        //       RE-ENGINEERED  na

        // PURPOSE OF THIS SUBROUTINE:
        // Using the UCSD UFAD exterior zone model, this subroutine calculates the  occupied subzone height,
        // surface heat transfer coefficients, the occupied subzone temperature, and the upper subzone temperature.

        // METHODOLOGY EMPLOYED:
        // The zone is divided into 2 subzones with a variable transition height.

        // REFERENCES:
        // The model is described in the EnergyPlus Engineering Reference in Anna Liu's UCSD PhD thesis.

        // Using/Aliasing
        using DataZoneEquipment::ZoneEquipConfig;
        using Psychrometrics::PsyCpAirFnW;
        using Psychrometrics::PsyRhoAirFnPbTdbW;
        using ScheduleManager::GetCurrentScheduleValue;
        using namespace DataHeatBalFanSys;
        using DataHVACGlobals::TimeStepSys;
        using DataHVACGlobals::UseZoneTimeStepHistory;
        using InternalHeatGains::SumInternalConvectionGainsByTypes;
        using InternalHeatGains::SumReturnAirConvectionGainsByTypes;

        // Locals
        // SUBROUTINE ARGUMENT DEFINITIONS:

        // SUBROUTINE PARAMETER DEFINITIONS:
        // na

        // INTERFACE BLOCK SPECIFICATIONS:
        // na

        // DERIVED TYPE DEFINITIONS:
        // na

        // SUBROUTINE LOCAL VARIABLE DECLARATIONS:
        static bool MIXFLAG(false); // if true treat as a mixed zone
        Real64 CeilingHeight;       // zone ceiling height above floor [m]
        int UINum;                  // index to underfloor interior zone model data
        Real64 GainsFrac;           // fraction of occupied subzone heat gains that remain in the subzone;
        // that is, don't go into the plumes
        Real64 HeightThermostat;    // height of the thermostat above the floor [m]
        Real64 HeightComfort;       // height at which comfort temperature is calculated
        Real64 TempDiffCritRep;     // Minimum temperature difference between upper and occupied subzones for reporting
        Real64 ConvGainsOccSubzone; // convective heat gains into the lower (occupied) subzone [W]
        Real64 ConvGainsUpSubzone;  // convective heat gains into the upper subzone [W]
        Real64 ConvGains;           // total zone convective gains (excluding surfaces) [W]
        Real64 ConvGainsWindows;    // convective gain from windows [W]
        int ZoneEquipConfigNum;     // ZoneEquipConfig index for this UFAD zone
        Real64 SumSysMCp;           // Sum of system mass flow rate * specific heat for this zone [W/K]
        Real64 SumSysMCpT;          // Sum of system mass flow rate * specific heat * temperature for this zone [W]
        Real64 SumSysM;             // Sum of systems mass flow rate [kg/s]
        Real64 NodeTemp;            // inlet node temperature [K]
        Real64 MassFlowRate;        // system mass flow rate [kg/s]
        Real64 CpAir;               // specific heat of air [J/kgK]
        int InNodeIndex;            // inlet node index in ZoneEquipConfig
        Real64 SumMCp;              // mass flow rate * specific heat for this zone for infiltration, ventilation, mixing [W/K]
        Real64 SumMCpT;             // mass flow rate * specific heat* temp for this zone for infiltration, ventilation, mixing [W]
        Real64 MCp_Total;           // total mass flow rate * specific heat for this zone [W/K]
        Real64 MCpT_Total;          // total mass flow rate * specific heat* temp for this zone [W]
        Real64 NumberOfPlumes;
        Real64 PowerInPlumes;             // [W]
        static Real64 PowerPerPlume(0.0); // power carried by each plume [W]
        Real64 PowerInPlumesPerMeter;     // Power in Plumes per meter of window length [W/m]
        static Real64 NumDiffusersPerPlume(0.0);
        Real64 HeightFrac; // Fractional height of transition between occupied and upper subzones
        Real64 TotSysFlow; // [m3/s]
        Real64 NumDiffusers;
        Real64 TSupK; // supply yemperature [K]
        Real64 Gamma; // dimensionless height parameter; higher gamma means interface height will be
        // higher, smaller gamma means interface height will be lower.
        Real64 DiffArea;     // diffuser effective area [m2]
        Real64 ThrowAngle;   // diffuser slot angle relative to vertical [radians]
        Real64 SourceHeight; // height of plume sources above the floor [m]
        int Ctd;
        Real64 AirCap;
        Real64 TempHistTerm;
        Real64 ZTAveraged;
        Real64 HeightUpSubzoneAve;       // Height of center of upper air subzone
        Real64 HeightOccupiedSubzoneAve; // Height of center of occupied air subzone
        Real64 ZoneMult;                 // total zone multiplier
        int ZoneNodeNum;                 // node number of the HVAC zone node
        static Real64 TempDepCoef(0.0);  // Formerly CoefSumha, coef in zone temp equation with dimensions of h*A
        static Real64 TempIndCoef(0.0);  // Formerly CoefSumhat, coef in zone temp equation with dimensions of h*A(T1
        static Array1D_int IntGainTypesOccupied(29,
                                                {IntGainTypeOf_People,
                                                 IntGainTypeOf_WaterHeaterMixed,
                                                 IntGainTypeOf_WaterHeaterStratified,
                                                 IntGainTypeOf_ThermalStorageChilledWaterMixed,
                                                 IntGainTypeOf_ThermalStorageChilledWaterStratified,
                                                 IntGainTypeOf_ElectricEquipment,
                                                 IntGainTypeOf_ElectricEquipmentITEAirCooled,
                                                 IntGainTypeOf_GasEquipment,
                                                 IntGainTypeOf_HotWaterEquipment,
                                                 IntGainTypeOf_SteamEquipment,
                                                 IntGainTypeOf_OtherEquipment,
                                                 IntGainTypeOf_ZoneBaseboardOutdoorTemperatureControlled,
                                                 IntGainTypeOf_GeneratorFuelCell,
                                                 IntGainTypeOf_WaterUseEquipment,
                                                 IntGainTypeOf_GeneratorMicroCHP,
                                                 IntGainTypeOf_ElectricLoadCenterTransformer,
                                                 IntGainTypeOf_ElectricLoadCenterInverterSimple,
                                                 IntGainTypeOf_ElectricLoadCenterInverterFunctionOfPower,
                                                 IntGainTypeOf_ElectricLoadCenterInverterLookUpTable,
                                                 IntGainTypeOf_ElectricLoadCenterStorageBattery,
                                                 IntGainTypeOf_ElectricLoadCenterStorageSimple,
                                                 IntGainTypeOf_PipeIndoor,
                                                 IntGainTypeOf_RefrigerationCase,
                                                 IntGainTypeOf_RefrigerationCompressorRack,
                                                 IntGainTypeOf_RefrigerationSystemAirCooledCondenser,
                                                 IntGainTypeOf_RefrigerationSystemSuctionPipe,
                                                 IntGainTypeOf_RefrigerationSecondaryReceiver,
                                                 IntGainTypeOf_RefrigerationSecondaryPipe,
                                                 IntGainTypeOf_RefrigerationWalkIn});

        static Array1D_int IntGainTypesUpSubzone(2, {IntGainTypeOf_DaylightingDeviceTubular, IntGainTypeOf_Lights});
        Real64 RetAirGains;

        // Exact solution or Euler method
        if (ZoneAirSolutionAlgo != Use3rdOrder) {
            if (ShortenTimeStepSysRoomAir && TimeStepSys < state.dataGlobal->TimeStepZone) {
                if (PreviousTimeStep < state.dataGlobal->TimeStepZone) {
                    Zone1OC(ZoneNum) = ZoneM2OC(ZoneNum);
                    Zone1MX(ZoneNum) = ZoneM2MX(ZoneNum);
                } else {
                    Zone1OC(ZoneNum) = ZoneMXOC(ZoneNum);
                    Zone1MX(ZoneNum) = ZoneMXMX(ZoneNum);
                }
            } else {
                Zone1OC(ZoneNum) = ZTOC(ZoneNum);
                Zone1MX(ZoneNum) = ZTMX(ZoneNum);
            }
        }

        HeightFrac = 0.0;
        MIXFLAG = false;
        UFHcIn = HConvIn;
        SumSysMCp = 0.0;
        SumSysMCpT = 0.0;
        TotSysFlow = 0.0;
        TSupK = 0.0;
        SumSysM = 0.0;
        PowerInPlumes = 0.0;
        ConvGainsWindows = 0.0;
        Gamma = 0.0;
        ZoneMult = Zone(ZoneNum).Multiplier * Zone(ZoneNum).ListMultiplier;
        CeilingHeight = ZoneCeilingHeight((ZoneNum - 1) * 2 + 2) - ZoneCeilingHeight((ZoneNum - 1) * 2 + 1);
        UINum = ZoneUFPtr(ZoneNum);
        HeightThermostat = ZoneUCSDUE(UINum).ThermostatHeight;
        HeightComfort = ZoneUCSDUE(UINum).ComfortHeight;
        TempDiffCritRep = ZoneUCSDUE(UINum).TempTrigger;
        DiffArea = ZoneUCSDUE(UINum).DiffArea;
        ThrowAngle = DataGlobalConstants::DegToRadians() * ZoneUCSDUE(UINum).DiffAngle;
        SourceHeight = ZoneUCSDUE(UINum).HeatSrcHeight;
        NumDiffusers = ZoneUCSDUE(UINum).DiffusersPerZone;
        PowerPerPlume = ZoneUCSDUE(UINum).PowerPerPlume;
        // gains from occupants, task lighting, elec equip, gas equip, other equip, hot water equip, steam equip,
        // baseboards (nonthermostatic), water heater skin loss
        SumInternalConvectionGainsByTypes(ZoneNum, IntGainTypesOccupied, ConvGainsOccSubzone);

        // Add heat to return air if zonal system (no return air) or cycling system (return air frequently very
        // low or zero)
        if (Zone(ZoneNum).NoHeatToReturnAir) {
            SumReturnAirConvectionGainsByTypes(ZoneNum, IntGainTypesOccupied, RetAirGains);
            ConvGainsOccSubzone += RetAirGains;
        }

        // Add convection from pool cover to occupied region
        ConvGainsOccSubzone += SumConvPool(ZoneNum);

        // gains from lights (ceiling), tubular daylighting devices, high temp radiant heaters
        SumInternalConvectionGainsByTypes(ZoneNum, IntGainTypesUpSubzone, ConvGainsUpSubzone);
        ConvGainsUpSubzone += SumConvHTRadSys(ZoneNum);
        if (Zone(ZoneNum).NoHeatToReturnAir) {
            SumReturnAirConvectionGainsByTypes(ZoneNum, IntGainTypesUpSubzone, RetAirGains);
            ConvGainsUpSubzone += RetAirGains;
        }
        ConvGains = ConvGainsOccSubzone + ConvGainsUpSubzone + SysDepZoneLoadsLagged(ZoneNum);
        ZoneEquipConfigNum = ZoneUCSDUE(UINum).ZoneEquipPtr;
        if (ZoneEquipConfigNum > 0) {
            for (InNodeIndex = 1; InNodeIndex <= ZoneEquipConfig(ZoneEquipConfigNum).NumInletNodes; ++InNodeIndex) {
                NodeTemp = Node(ZoneEquipConfig(ZoneEquipConfigNum).InletNode(InNodeIndex)).Temp;
                MassFlowRate = Node(ZoneEquipConfig(ZoneEquipConfigNum).InletNode(InNodeIndex)).MassFlowRate;
                CpAir = PsyCpAirFnW(ZoneAirHumRat(ZoneNum));
                SumSysMCp += MassFlowRate * CpAir;
                SumSysMCpT += MassFlowRate * CpAir * NodeTemp;
                TotSysFlow += MassFlowRate / PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, NodeTemp, ZoneAirHumRat(ZoneNum));
                TSupK += MassFlowRate * NodeTemp;
                SumSysM += MassFlowRate;
            }
            if (TotSysFlow > 0.0) {
                TSupK = TSupK / SumSysM + DataGlobalConstants::KelvinConv();
            } else {
                TSupK = 0.0;
            }
        }
        // mass flow times specific heat for infiltration, ventilation, mixing
        SumMCp = MCPI(ZoneNum) + MCPV(ZoneNum) + MCPM(ZoneNum) + MDotCPOA(ZoneNum);
        // mass flow times specific heat times temperature for infiltration, ventilation, mixing
        SumMCpT = MCPTI(ZoneNum) + MCPTV(ZoneNum) + MCPTM(ZoneNum) + MDotCPOA(ZoneNum) * Zone(ZoneNum).OutDryBulbTemp;

        MCp_Total = SumMCp + SumSysMCp;
        MCpT_Total = SumMCpT + SumSysMCpT;

        // For the York MIT diffusers (variable area) the area varies with the flow rate. Assume 400 ft/min velocity
        // at the diffuser, and a design flow rate of 150 cfm (.0708 m3/s). Then the design area for each diffuser is
        // 150 ft3/min / 400 ft/min = .375 ft2 = .035 m2. This is adjusted each time step by
        //               (TotSysFlow/(NumDiffusers*.0708))*.035
        if (ZoneUCSDUE(UINum).DiffuserType == VarArea) {
            DiffArea = 0.035 * TotSysFlow / (0.0708 * NumDiffusers);
        }
        // initial estimate of convective transfer from surfaces; assume HeightFrac is 0.5.
        HcUCSDUF(state, ZoneNum, 0.5);
        ConvGainsWindows = state.dataUFADManager->HAT_MXWin + state.dataUFADManager->HAT_OCWin - state.dataUFADManager->HA_MXWin * ZTMX(ZoneNum) - state.dataUFADManager->HA_OCWin * ZTOC(ZoneNum);
        PowerInPlumes = ConvGains + state.dataUFADManager->HAT_OC - state.dataUFADManager->HA_OC * ZTOC(ZoneNum) + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum);
        // NumberOfPlumes = PowerInPlumes / PowerPerPlume
        if (PowerPerPlume > 0.0 && PowerInPlumes > 0.0) {
            NumberOfPlumes = PowerInPlumes / PowerPerPlume;
            NumDiffusersPerPlume = NumDiffusers / NumberOfPlumes;
        } else {
            NumberOfPlumes = 1.0;
            NumDiffusersPerPlume = 1.0;
        }
        if ((PowerInPlumes <= 0.0) || (TotSysFlow == 0.0) || (TSupK - DataGlobalConstants::KelvinConv()) > MAT(ZoneNum)) {
            // The system will mix
            HeightFrac = 0.0;
        } else {
            if (PowerInPlumes > 0.0) {
                if (ZoneUCSDUE(UINum).WinWidth > 0.0) { // exterior zone formula
                    PowerInPlumesPerMeter = PowerInPlumes / ZoneUCSDUE(UINum).WinWidth;
                    Gamma =
                        (TotSysFlow * std::cos(ThrowAngle)) / (NumDiffusers * DiffArea * std::pow(0.0281 * 0.001 * PowerInPlumesPerMeter, 0.333333));
                } else { // interior zone formula
                    Gamma = std::pow(TotSysFlow * std::cos(ThrowAngle), 1.5) /
                            (NumberOfPlumes * std::pow(NumDiffusersPerPlume * DiffArea, 1.25) * std::sqrt(0.0281 * 0.001 * PowerInPlumes));
                }
            } else {
                Gamma = 1000.0;
            }
            if (ZoneUCSDUE(UINum).CalcTransHeight) {
                if (ZoneUCSDUE(UINum).WinWidth > 0.0) { // use exterior zone formula
                    HeightFrac = (std::sqrt(DiffArea) * (11.03 * std::log(Gamma) - 10.73) + 0.5 * SourceHeight) / CeilingHeight;
                } else { // use interior zone formula
                    HeightFrac = (std::sqrt(NumDiffusersPerPlume * DiffArea) * (7.43 * std::log(Gamma) - 1.35) + 0.5 * SourceHeight) / CeilingHeight;
                }
            } else {
                HeightFrac = ZoneUCSDUE(UINum).TransHeight / CeilingHeight;
            }
            HeightFrac = max(0.0, min(1.0, HeightFrac));
            GainsFrac = ZoneUCSDUE(UINum).A_Kc * std::pow(Gamma, ZoneUCSDUE(UINum).B_Kc) + ZoneUCSDUE(UINum).C_Kc + ZoneUCSDUE(UINum).D_Kc * Gamma +
                        ZoneUCSDUE(UINum).E_Kc * pow_2(Gamma);
            GainsFrac = max(0.7, min(GainsFrac, 1.0));
            if (ZoneUCSDUE(UINum).ShadeDown) {
                GainsFrac -= 0.2;
            }
            ZoneUFPowInPlumes(ZoneNum) = PowerInPlumes;
            for (Ctd = 1; Ctd <= 4; ++Ctd) {
                HcUCSDUF(state, ZoneNum, HeightFrac);
                ConvGainsWindows = state.dataUFADManager->HAT_MXWin + state.dataUFADManager->HAT_OCWin - state.dataUFADManager->HA_MXWin * ZTMX(ZoneNum) - state.dataUFADManager->HA_OCWin * ZTOC(ZoneNum);
                ConvGainsWindows = max(ConvGainsWindows, 0.0);
                PowerInPlumes = ConvGains + state.dataUFADManager->HAT_OC - state.dataUFADManager->HA_OC * ZTOC(ZoneNum) + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum);
                // NumberOfPlumes = PowerInPlumes / PowerPerPlume
                NumberOfPlumes = 1.0;
                if (PowerInPlumes <= 0.0) break;
                if (ZoneUCSDUE(UINum).WinWidth > 0.0) { // use exterior zone formula
                    PowerInPlumesPerMeter = PowerInPlumes / ZoneUCSDUE(UINum).WinWidth;
                    Gamma =
                        (TotSysFlow * std::cos(ThrowAngle)) / (NumDiffusers * DiffArea * std::pow(0.0281 * 0.001 * PowerInPlumesPerMeter, 0.333333));
                } else { // use interior zone formula
                    Gamma = std::pow(TotSysFlow * std::cos(ThrowAngle), 1.5) /
                            (NumberOfPlumes * std::pow(NumDiffusersPerPlume * DiffArea, 1.25) * std::sqrt(0.0281 * 0.001 * PowerInPlumes));
                }
                if (ZoneUCSDUE(UINum).CalcTransHeight) {
                    if (ZoneUCSDUE(UINum).WinWidth > 0.0) { // exterior zone formula
                        HeightFrac = (std::sqrt(DiffArea) * (11.03 * std::log(Gamma) - 10.73) + 0.5 * SourceHeight) / CeilingHeight;
                    } else { // interior zone formula
                        HeightFrac =
                            (std::sqrt(NumDiffusersPerPlume * DiffArea) * (7.43 * std::log(Gamma) - 1.35) + 0.5 * SourceHeight) / CeilingHeight;
                    }
                } else {
                    HeightFrac = ZoneUCSDUE(UINum).TransHeight / CeilingHeight;
                }
                HeightFrac = min(1.0, HeightFrac);
                HeightTransition(ZoneNum) = HeightFrac * CeilingHeight;
                GainsFrac = ZoneUCSDUE(UINum).A_Kc * std::pow(Gamma, ZoneUCSDUE(UINum).B_Kc) + ZoneUCSDUE(UINum).C_Kc +
                            ZoneUCSDUE(UINum).D_Kc * Gamma + ZoneUCSDUE(UINum).E_Kc * pow_2(Gamma);
                GainsFrac = max(0.7, min(GainsFrac, 1.0));
                if (ZoneUCSDUE(UINum).ShadeDown) {
                    GainsFrac -= 0.2;
                }
                AIRRATOC(ZoneNum) = Zone(ZoneNum).Volume * (HeightTransition(ZoneNum) - min(HeightTransition(ZoneNum), 0.2)) / CeilingHeight *
                                    Zone(ZoneNum).ZoneVolCapMultpSens * PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, MATOC(ZoneNum), ZoneAirHumRat(ZoneNum)) *
                                    PsyCpAirFnW(ZoneAirHumRat(ZoneNum)) / (TimeStepSys * DataGlobalConstants::SecInHour());
                AIRRATMX(ZoneNum) = Zone(ZoneNum).Volume * (CeilingHeight - HeightTransition(ZoneNum)) / CeilingHeight *
                                    Zone(ZoneNum).ZoneVolCapMultpSens * PsyRhoAirFnPbTdbW(state, state.dataEnvrn->OutBaroPress, MATMX(ZoneNum), ZoneAirHumRat(ZoneNum)) *
                                    PsyCpAirFnW(ZoneAirHumRat(ZoneNum)) / (TimeStepSys * DataGlobalConstants::SecInHour());

                if (UseZoneTimeStepHistory) {
                    ZTM3OC(ZoneNum) = XM3TOC(ZoneNum);
                    ZTM2OC(ZoneNum) = XM2TOC(ZoneNum);
                    ZTM1OC(ZoneNum) = XMATOC(ZoneNum);

                    ZTM3MX(ZoneNum) = XM3TMX(ZoneNum);
                    ZTM2MX(ZoneNum) = XM2TMX(ZoneNum);
                    ZTM1MX(ZoneNum) = XMATMX(ZoneNum);

                } else {
                    ZTM3OC(ZoneNum) = DSXM3TOC(ZoneNum);
                    ZTM2OC(ZoneNum) = DSXM2TOC(ZoneNum);
                    ZTM1OC(ZoneNum) = DSXMATOC(ZoneNum);

                    ZTM3MX(ZoneNum) = DSXM3TMX(ZoneNum);
                    ZTM2MX(ZoneNum) = DSXM2TMX(ZoneNum);
                    ZTM1MX(ZoneNum) = DSXMATMX(ZoneNum);
                }

                AirCap = AIRRATOC(ZoneNum);
                TempHistTerm = AirCap * (3.0 * ZTM1OC(ZoneNum) - (3.0 / 2.0) * ZTM2OC(ZoneNum) + (1.0 / 3.0) * ZTM3OC(ZoneNum));
                TempDepCoef = GainsFrac * state.dataUFADManager->HA_OC + MCp_Total;
                TempIndCoef =
                    GainsFrac * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum)) + MCpT_Total + NonAirSystemResponse(ZoneNum) / ZoneMult;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTOC(ZoneNum) = (TempHistTerm + GainsFrac * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_MX * ZTMX(ZoneNum)) + MCpT_Total +
                                         NonAirSystemResponse(ZoneNum) / ZoneMult) /
                                        ((11.0 / 6.0) * AirCap + GainsFrac * state.dataUFADManager->HA_OC + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTOC(ZoneNum) = Zone1OC(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTOC(ZoneNum) = (Zone1OC(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                            TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTOC(ZoneNum) = (AirCap * Zone1OC(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                AirCap = AIRRATMX(ZoneNum);
                TempHistTerm = AirCap * (3.0 * ZTM1MX(ZoneNum) - (3.0 / 2.0) * ZTM2MX(ZoneNum) + (1.0 / 3.0) * ZTM3MX(ZoneNum));
                TempDepCoef = (1.0 - GainsFrac) * state.dataUFADManager->HA_MX + MCp_Total;
                TempIndCoef = (1.0 - GainsFrac) * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_OC * ZTOC(ZoneNum)) + ZTOC(ZoneNum) * MCp_Total;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTMX(ZoneNum) =
                            (TempHistTerm + (1.0 - GainsFrac) * (ConvGains + state.dataUFADManager->HAT_OC + state.dataUFADManager->HAT_MX - state.dataUFADManager->HA_OC * ZTOC(ZoneNum)) + ZTOC(ZoneNum) * MCp_Total) /
                            ((11.0 / 6.0) * AirCap + (1.0 - GainsFrac) * state.dataUFADManager->HA_MX + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTMX(ZoneNum) = Zone1MX(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTMX(ZoneNum) = (Zone1MX(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                            TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTMX(ZoneNum) = (AirCap * Zone1MX(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                ZTFloor(ZoneNum) = ZTOC(ZoneNum);
            }
            if (PowerInPlumes <= 0.0) {
                HeightFrac = 0.0;
                AirModel(ZoneNum).SimAirModel = false;
                ZoneUFGamma(ZoneNum) = 0.0;
                ZoneUFPowInPlumes(ZoneNum) = 0.0;
                ZoneUFPowInPlumesfromWindows(ZoneNum) = 0.0;
            } else {
                AirModel(ZoneNum).SimAirModel = true;
                ZoneUFGamma(ZoneNum) = Gamma;
                ZoneUFPowInPlumes(ZoneNum) = PowerInPlumes;
                ZoneUFPowInPlumesfromWindows(ZoneNum) = ConvGainsWindows;
            }
        }

        //=============================== M I X E D  Calculation ==============================================
        if (ZTMX(ZoneNum) < ZTOC(ZoneNum) || MCp_Total <= 0.0 || HeightFrac * CeilingHeight < state.dataUFADManager->ThickOccupiedSubzoneMin) {
            MIXFLAG = true;
            HeightFrac = 0.0;

            AvgTempGrad(ZoneNum) = 0.0;
            MaxTempGrad(ZoneNum) = 0.0;
            AirModel(ZoneNum).SimAirModel = false;
            AirCap = AIRRAT(ZoneNum);
            TempHistTerm = AirCap * (3.0 * ZTM1(ZoneNum) - (3.0 / 2.0) * ZTM2(ZoneNum) + (1.0 / 3.0) * ZTM3(ZoneNum));

            for (Ctd = 1; Ctd <= 3; ++Ctd) {
                TempDepCoef = state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total;
                TempIndCoef = ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTAveraged = (TempHistTerm + ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total) / ((11.0 / 6.0) * AirCap + state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTAveraged = ZoneT1(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTAveraged = (ZoneT1(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                         TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTAveraged = (AirCap * ZoneT1(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                ZTOC(ZoneNum) = ZTAveraged;
                ZTMX(ZoneNum) = ZTAveraged;
                ZTFloor(ZoneNum) = ZTAveraged;
                HcUCSDUF(state, ZoneNum, HeightFrac);
                TempDepCoef = state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total;
                TempIndCoef = ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total;
                {
                    auto const SELECT_CASE_var(ZoneAirSolutionAlgo);
                    if (SELECT_CASE_var == Use3rdOrder) {
                        ZTAveraged = (TempHistTerm + ConvGains + state.dataUFADManager->HAT_MX + state.dataUFADManager->HAT_OC + MCpT_Total) / ((11.0 / 6.0) * AirCap + state.dataUFADManager->HA_MX + state.dataUFADManager->HA_OC + MCp_Total);
                    } else if (SELECT_CASE_var == UseAnalyticalSolution) {
                        if (TempDepCoef == 0.0) { // B=0
                            ZTAveraged = ZoneT1(ZoneNum) + TempIndCoef / AirCap;
                        } else {
                            ZTAveraged = (ZoneT1(ZoneNum) - TempIndCoef / TempDepCoef) * std::exp(min(700.0, -TempDepCoef / AirCap)) +
                                         TempIndCoef / TempDepCoef;
                        }
                    } else if (SELECT_CASE_var == UseEulerMethod) {
                        ZTAveraged = (AirCap * ZoneT1(ZoneNum) + TempIndCoef) / (AirCap + TempDepCoef);
                    }
                }
                ZTOC(ZoneNum) = ZTAveraged;
                ZTMX(ZoneNum) = ZTAveraged;
                ZTFloor(ZoneNum) = ZTAveraged;
            }
        }
        //=========================================================================================

        // Comfort temperature and temperature at the thermostat/temperature control sensor

        HeightUpSubzoneAve = (CeilingHeight + HeightTransition(ZoneNum)) / 2.0;
        HeightOccupiedSubzoneAve = HeightTransition(ZoneNum) / 2.0;
        // Comfort temperature

        if (MIXFLAG) {
            TCMF(ZoneNum) = ZTAveraged;
        } else {
            if (HeightComfort < HeightOccupiedSubzoneAve) {
                TCMF(ZoneNum) = ZTOC(ZoneNum);
            } else if (HeightComfort >= HeightOccupiedSubzoneAve && HeightComfort < HeightUpSubzoneAve) {
                TCMF(ZoneNum) = (ZTOC(ZoneNum) * (HeightUpSubzoneAve - HeightComfort) + ZTMX(ZoneNum) * (HeightComfort - HeightOccupiedSubzoneAve)) /
                                (HeightUpSubzoneAve - HeightOccupiedSubzoneAve);
            } else if (HeightComfort >= HeightUpSubzoneAve && HeightComfort <= CeilingHeight) {
                TCMF(ZoneNum) = ZTMX(ZoneNum);
            } else {
                ShowFatalError(state, "UFAD comfort height is above ceiling or below floor in Zone: " + Zone(ZoneNum).Name);
            }
        }

        // Temperature at the thermostat/temperature control sensor

        if (MIXFLAG) {
            TempTstatAir(ZoneNum) = ZTAveraged;
        } else {
            if (HeightThermostat < HeightOccupiedSubzoneAve) {
                TempTstatAir(ZoneNum) = ZTOC(ZoneNum);
            } else if (HeightThermostat >= HeightOccupiedSubzoneAve && HeightThermostat < HeightUpSubzoneAve) {
                TempTstatAir(ZoneNum) =
                    (ZTOC(ZoneNum) * (HeightUpSubzoneAve - HeightThermostat) + ZTMX(ZoneNum) * (HeightThermostat - HeightOccupiedSubzoneAve)) /
                    (HeightUpSubzoneAve - HeightOccupiedSubzoneAve);
            } else if (HeightThermostat >= HeightUpSubzoneAve && HeightThermostat <= CeilingHeight) {
                TempTstatAir(ZoneNum) = ZTMX(ZoneNum);
            } else {
                ShowFatalError(state, "Underfloor air distribution thermostat height is above ceiling or below floor in Zone: " + Zone(ZoneNum).Name);
            }
        }

        // Temperature gradients
        if ((HeightUpSubzoneAve - HeightOccupiedSubzoneAve) > 0.1) {
            AvgTempGrad(ZoneNum) = (ZTMX(ZoneNum) - ZTOC(ZoneNum)) / (HeightUpSubzoneAve - HeightOccupiedSubzoneAve);
        } else {
            AvgTempGrad(ZoneNum) = 0.0;
        }

        if (MIXFLAG) {
            ZoneUFMixedFlag(ZoneNum) = 1;
            AirModel(ZoneNum).SimAirModel = false;
        } else {
            ZoneUFMixedFlag(ZoneNum) = 0;
            AirModel(ZoneNum).SimAirModel = true;
        }

        if (ZoneEquipConfigNum > 0) {
            ZoneNodeNum = Zone(ZoneNum).SystemZoneNodeNumber;
            Node(ZoneNodeNum).Temp = ZTMX(ZoneNum);
        }

        if (MIXFLAG) {
            Phi(ZoneNum) = 1.0;
        } else {
            Phi(ZoneNum) = (ZTOC(ZoneNum) - (TSupK - DataGlobalConstants::KelvinConv())) / (ZTMX(ZoneNum) - (TSupK - DataGlobalConstants::KelvinConv()));
        }

        // Mixed for reporting purposes
        if ((MIXFLAG) || ((ZTMX(ZoneNum) - ZTOC(ZoneNum)) < TempDiffCritRep)) {
            ZoneUFMixedFlagRep(ZoneNum) = 1.0;
            HeightTransition(ZoneNum) = 0.0;
            AvgTempGrad(ZoneNum) = 0.0;
        } else {
            ZoneUFMixedFlagRep(ZoneNum) = 0.0;
        }
    }

} // namespace UFADManager

} // namespace EnergyPlus
