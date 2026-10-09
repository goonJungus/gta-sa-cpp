// eModelID.h - canonical vehicle/object model ID enumeration.
// Created 2026-10-09: replaces the ad-hoc constexpr/anonymous-enum stand-ins
// scattered across CVehicle.h (7x constexpr), CAutomobile.cpp, CBoat.cpp,
// CTrain.cpp, CPlane.cpp. All values verified 2026-10-09 against the decomp
// (src_prev_export/_types.h). The full gta-reversed enum is 14832 entries;
// this file grows as the port needs more IDs. When adding, verify against
// src_prev_export/_types.h and note the date.
#pragma once
#include <cstdint>

enum eModelID : int32_t {
    MODEL_INVALID = -1,
    UNLOAD_MODEL  = -2, // not a real model; sentinel used by collision code (gta-reversed)

    // Vehicle model IDs used by the vehicle subsystem (verified 2026-10-09)
    MODEL_LINERUN  = 403,
    MODEL_DUMPER   = 406,
    MODEL_LEVIATHN = 417,
    MODEL_TAXI     = 420,
    MODEL_SECURICA = 428,
    MODEL_RHINO    = 432,
    MODEL_BARRACKS = 433,
    MODEL_CABBIE   = 438,
    MODEL_PACKER   = 443,
    MODEL_SEASPAR  = 447,
    MODEL_RCRAIDER = 465,
    MODEL_BAGGAGE  = 485,
    MODEL_DOZER    = 486,
    MODEL_VCNMAV   = 488,
    MODEL_RCGOBLIN = 501,
    MODEL_PETRO    = 514,
    MODEL_RDTRAIN  = 515,
    MODEL_TOWTRUCK = 525,
    MODEL_FORKLIFT = 530,
    MODEL_TRACTOR  = 531,
    MODEL_UTILITY  = 552,
    MODEL_RCTIGER  = 564,
    MODEL_KART     = 571,
    MODEL_TUG      = 583,
    MODEL_ARTICT3  = 591,
    MODEL_ANDROM   = 592,
    MODEL_BAGBOXA  = 606,
    MODEL_BAGBOXB  = 607,
    MODEL_TUGSTAIR = 608,
    MODEL_STREAKC  = 570,

    // Boat/train model IDs (verified 2026-10-09 vs src_prev_export/_types.h)
    MODEL_MARQUIS  = 484,

    // Plane model IDs (verified 2026-10-09 vs src_prev_export/_types.h)
    MODEL_SKIMMER  = 460,
    MODEL_RUSTLER  = 476,
    MODEL_CROPDUST = 512,
    MODEL_STUNT    = 513,
    MODEL_SHAMAL   = 519,
    MODEL_HYDRA    = 520,
    MODEL_VORTEX   = 539,
    MODEL_RCBANDIT = 441,
    MODEL_RCCAM    = 594,
    MODEL_NEVADA   = 553,
    MODEL_AT400    = 577,

    // Temp-collision model IDs (verified 2026-10-09 vs src_prev_export/_types.h)
    MODEL_TEMPCOL_DOOR1     = 374,
    MODEL_TEMPCOL_BUMPER1   = 375, // gta-reversed eModelID.h (added 2026-10-09)
    MODEL_TEMPCOL_PANEL1    = 376, // gta-reversed eModelID.h (added 2026-10-09)
    MODEL_TEMPCOL_BONNET1   = 377, // gta-reversed eModelID.h (added 2026-10-09)
    MODEL_TEMPCOL_BOOT1     = 378, // gta-reversed eModelID.h (added 2026-10-09)
    MODEL_TEMPCOL_WHEEL1    = 379, // gta-reversed eModelID.h (added 2026-10-09)
    MODEL_TEMPCOL_BODYPART1 = 380, // gta-reversed eModelID.h (added 2026-10-09)
    MODEL_TEMPCOL_BODYPART2 = 381,

    // General vehicle/object IDs (verified 2026-10-09 vs src_prev_export/_types.h)
    MODEL_GOLFCLUB = 333,
    MODEL_CHROMEGUN = 349,
    MODEL_STRETCH  = 409,
    MODEL_AMBULAN  = 416,
    MODEL_HUNTER   = 425,
    MODEL_ENFORCER = 427,
    MODEL_PREDATOR = 430,
    MODEL_COACH    = 437,
    MODEL_REEFER   = 453,
    MODEL_TROPIC   = 454,
    MODEL_CADDY    = 457,
    MODEL_RCBARON  = 464,
    MODEL_BEAGLE   = 511,
    MODEL_TORNADO  = 576,
    MODEL_HOTDOG   = 588,
    MODEL_COPCARLA = 596,
    MODEL_COPCARSF = 597,
    MODEL_COPCARVG = 598,
    MODEL_COPCARRU = 599,

    // Vehicle IDs needed by CAutomobile (verified 2026-10-09 vs gta-reversed Enums/eModelID.h)
    MODEL_ESPERANT = 419,
    MODEL_MRWHOOP  = 423,
    MODEL_CEMENT   = 524,
    MODEL_COMBINE  = 532,
    MODEL_FIRELA   = 544,
    MODEL_FIRETRUK = 407,
    MODEL_SWATVAN  = 601,
    MODEL_SANDKING = 495,
    MODEL_BFINJECT = 424,
    MODEL_COMET    = 480,
    MODEL_STALLION = 439,
    MODEL_FARMTR1  = 610,
    MODEL_UTILTR1  = 611,
};
