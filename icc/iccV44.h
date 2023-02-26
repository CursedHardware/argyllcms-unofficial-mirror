
/*
 * This file started out as the standard ICC header provided by
 * the ICC. Since this file no longer seems to be maintained,
 * I've added new entries from the ICC spec., but dropped
 * things that are not needed for icclib.
 *
 * This version of the header file corresponds to the profile
 * Specification ICC.1:2022 (Version 4.4.0)
 *
 * All header file entries are pre-fixed with "ic" to help 
 * avoid name space collisions. Signatures are pre-fixed with
 * icSig. icclib specific items are prefixed "icm".
 *
 * Version numbers used within file are file format version numbers,
 * not spec. version numbers.
 *
 * Portions of this file are Copyright 2004 - 2022 Graeme W. Gill,
 * This material is licensed with an "MIT" free use license:-
 * see the License4.txt file in this directory for licensing details.
 *
 *  Graeme Gill.
 */

/* Header file guard bands */
#ifndef ICCV44_H
#define ICCV44_H

#ifdef __cplusplus
	extern "C" {
#endif

/***************************************************************** 
 Copyright (c) 1994-1998 SunSoft, Inc.

                    Rights Reserved

Permission is hereby granted, free of charge, to any person 
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without restrict- 
ion, including without limitation the rights to use, copy, modify, 
merge, publish distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished 
to do so, subject to the following conditions: 
 
The above copyright notice and this permission notice shall be 
included in all copies or substantial portions of the Software. 
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-
INFRINGEMENT.  IN NO EVENT SHALL SUNSOFT, INC. OR ITS PARENT 
COMPANY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
OTHER DEALINGS IN THE SOFTWARE. 
 
Except as contained in this notice, the name of SunSoft, Inc. 
shall not be used in advertising or otherwise to promote the 
sale, use or other dealings in this Software without written 
authorization from SunSoft Inc. 
******************************************************************/

/* 
   Spec name/version/file version relationship.
   
   Spec. Name      Spec. Version    ICC File Version
   ----------      -------------    ----------------
   ICC.1:2022         4.4            4.4.0
   ICC.1:2010         4.3            4.3.0
   ICC.1:2004-10      4.2            4.2.0
   ICC.1:2003-09      4.1            4.1.0
   ICC.1:2001-12      4.0            4.0.0
   ICC.1:2001-04      (3.7?)         2.4.0
   ICC.1A:1999-04     (3.6?)         2.3.0
   ICC.1:1998-09      (3.5?)         2.2.0
   (Version 3.4)      3.4            2.1.0 (a)
   (Version 3.3)      3.3            2.1.0
   (Version 3.2)      3.2            2.0.0 (b) [Assumed to be official V2.0]
   (Version 3.01)     3.01           2.0.0 (a) [Preliminary]
   (Version 3.0)      3.0            2.0.0
*/


#define icMaxTagVal 0xFFFFFFFF 
#define icAlignSize 4

/*------------------------------------------------------------------------*/

/*
 * Defines used in the specification
 */

#define icMagicNumber                   0x61637370     /* 'acsp' */

#define icVersionNumberV20              0x02000000     /* 2.0.0 BCD */
#define icVersionNumberV21              0x02100000     /* 2.1.0 BCD */
#define icVersionNumberV22              0x02200000     /* 2.2.0 BCD */
#define icVersionNumberV23              0x02300000     /* 2.3.0 BCD */
#define icVersionNumberV24              0x02400000     /* 2.4.0 BCD */
#define icVersionNumberV40              0x04000000     /* 4.0.0 BCD */
#define icVersionNumberV41              0x04100000     /* 4.1.0 BCD */
#define icVersionNumberV42              0x04200000     /* 4.2.0 BCD */
#define icVersionNumberV43              0x04300000     /* 4.3.0 BCD */
#define icVersionNumberV44              0x04400000     /* 4.4.0 BCD */

/* Screening Encodings */
typedef unsigned int icScreening;
#define icPrtrDefaultScreensFalse       0x00000000     /* Bit pos 0 */
#define icPrtrDefaultScreensTrue        0x00000001     /* Bit pos 0 */
#define icLinesPerInch                  0x00000002     /* Bit pos 1 */
#define icLinesPerCm                    0x00000000     /* Bit pos 1 */

/* 
 * Device attributes, currently defined values correspond
 * to the least-significant 4 bytes of the 8 byte attribute
 * quantity, see the header for their location.
 */

typedef unsigned int icDeviceAttributes;    /* (the bottom 32 bits) */
#define icReflective                    0x00000000     /* Bit pos 0 */
#define icTransparency                  0x00000001     /* Bit pos 0 */
#define icGlossy                        0x00000000     /* Bit pos 1 */
#define icMatte                         0x00000002     /* Bit pos 1 */
#define icPositive                      0x00000000     /* Bit pos 2 */
#define icNegative                      0x00000004     /* Bit pos 2 */
#define icColor                         0x00000000     /* Bit pos 3 */
#define icBlackAndWhite                 0x00000008     /* Bit pos 3 */

/*
 * Profile header flags, the least-significant 16 bits are reserved
 * for consortium use.
 */

typedef unsigned int icProfileFlags;
#define icEmbeddedProfileFalse          0x00000000     /* Bit pos 0 */
#define icEmbeddedProfileTrue           0x00000001     /* Bit pos 0 */
#define icUseAnywhere                   0x00000000     /* Bit pos 1 */
#define icUseWithEmbeddedDataOnly       0x00000002     /* Bit pos 1 */

/* Ascii or Binary data */
typedef unsigned int icAsciiOrBinary;
#define icAsciiData                     0x00000000 
#define icBinaryData                    0x00000001

/* Video card gamma formats (ColorSync 2.5 specific) */
typedef unsigned int icVideoCardGammaFormat;
#define icVideoCardGammaTable           0x00000000
#define icVideoCardGammaFormula         0x00000001

/*------------------------------------------------------------------------*/
/* public tags and sizes (+ version notes) */
typedef enum {
	icSigAToB0Tag                          = 0x41324230,  /* 'A2B0' */ 
	icSigAToB1Tag                          = 0x41324231,  /* 'A2B1' */
	icSigAToB2Tag                          = 0x41324232,  /* 'A2B2' */ 
	icSigBlueMatrixColumnTag               = 0x6258595A,  /* 'bXYZ' */
	icSigBlueTRCTag                        = 0x62545243,  /* 'bTRC' */
	icSigBToA0Tag                          = 0x42324130,  /* 'B2A0' */
	icSigBToA1Tag                          = 0x42324131,  /* 'B2A1' */
	icSigBToA2Tag                          = 0x42324132,  /* 'B2A2' */
	icSigBToD0Tag                          = 0x42324430,  /* 'B2D0' V4.3+ */
	icSigBToD1Tag                          = 0x42324431,  /* 'B2D1' V4.3+ */
	icSigBToD2Tag                          = 0x42324432,  /* 'B2D2' V4.3+ */
	icSigBToD3Tag                          = 0x42324433,  /* 'B2D3' V4.3+ */
	icSigCalibrationDateTimeTag            = 0x63616C74,  /* 'calt' */
	icSigCharTargetTag                     = 0x74617267,  /* 'targ' */ 
	icSigChromaticAdaptationTag            = 0x63686164,  /* 'chad' V2.4+ */
	icSigChromaticityTag                   = 0x6368726D,  /* 'chrm' V2.3+ */ 
	icSigCicpTag                           = 0x63696370,  /* 'cicp' V4.4+ */ 
	icSigColorantOrderTag                  = 0x636C726F,  /* 'clro' V4.0+ */
	icSigColorantTableTag                  = 0x636C7274,  /* 'clrt' V4.0+ */
	icSigColorantTableOutTag               = 0x636C6F74,  /* 'clot' V4.0+ */
	icSigColorimetricIntentImageStateTag   = 0x63696973,  /* 'ciis' V4.3+ */
	icSigCopyrightTag                      = 0x63707274,  /* 'cprt' */
	icSigCrdInfoTag                        = 0x63726469,  /* 'crdi' V2.1 - V4.0 */
	icSigDataTag                           = 0x64617461,  /* 'data' ???? */
	icSigDateTimeTag                       = 0x6474696D,  /* 'dtim' ???? */
	icSigDeviceMfgDescTag                  = 0x646D6E64,  /* 'dmnd' */
	icSigDeviceModelDescTag                = 0x646D6464,  /* 'dmdd' */
	icSigDeviceSettingsTag                 = 0x64657673,  /* 'devs' V2.2 - V4.0 */
	icSigDToB0Tag                          = 0x44324230,  /* 'D2B0' V4.3+ */
	icSigDToB1Tag                          = 0x44324231,  /* 'D2B1' V4.3+ */
	icSigDToB2Tag                          = 0x44324232,  /* 'D2B2' V4.3+ */
	icSigDToB3Tag                          = 0x44324233,  /* 'D2B3' V4.3+ */
	icSigGamutTag                          = 0x67616D74,  /* 'gamt' */
	icSigGrayTRCTag                        = 0x6b545243,  /* 'kTRC' */
	icSigGreenMatrixColumnTag              = 0x6758595A,  /* 'gXYZ' */
	icSigGreenTRCTag                       = 0x67545243,  /* 'gTRC' */
	icSigLuminanceTag                      = 0x6C756d69,  /* 'lumi' */
	icSigMeasurementTag                    = 0x6D656173,  /* 'meas' */
	icSigMetadataTag                       = 0x6d657461,  /* 'meta' V4.4+ */
	icSigMediaBlackPointTag                = 0x626B7074,  /* 'bkpt' V2.0 - V4.2 */
	icSigMediaWhitePointTag                = 0x77747074,  /* 'wtpt' */
	icSigNamedColorTag                     = 0x6E636f6C,  /* 'ncol' V2.0 - V2.0*/
	icSigNamedColor2Tag                    = 0x6E636C32,  /* 'ncl2' V2.1+ */
	icSigOutputResponseTag                 = 0x72657370,  /* 'resp' V2.2+ */
	icSigPerceptualRenderingIntentGamutTag = 0x72696730,  /* 'rig0' V4.3+ */
	icSigPreview0Tag                       = 0x70726530,  /* 'pre0' */
	icSigPreview1Tag                       = 0x70726531,  /* 'pre1' */
	icSigPreview2Tag                       = 0x70726532,  /* 'pre2' */
	icSigProfileDescriptionTag             = 0x64657363,  /* 'desc' */
	icSigProfileSequenceDescTag            = 0x70736571,  /* 'pseq' */
	icSigProfileSequenceIdentifierTag      = 0x70736964,  /* 'psid' V4.3+ */
	icSigPs2CRD0Tag                        = 0x70736430,  /* 'psd0' V2.0 - V4.0 */
	icSigPs2CRD1Tag                        = 0x70736431,  /* 'psd1' V2.0 - V4.0 */
	icSigPs2CRD2Tag                        = 0x70736432,  /* 'psd2' V2.0 - V4.0 */
	icSigPs2CRD3Tag                        = 0x70736433,  /* 'psd3' V2.0 - V4.0 */
	icSigPs2CSATag                         = 0x70733273,  /* 'ps2s' V2.0 - V4.0 */
	icSigPs2RenderingIntentTag             = 0x70733269,  /* 'ps2i' V2.0 - V4.0 */
	icSigRedMatrixColumnTag                = 0x7258595A,  /* 'rXYZ' */
	icSigRedTRCTag                         = 0x72545243,  /* 'rTRC' */
	icSigSaturationRenderingIntentGamutTag = 0x72696732,  /* 'rig2' V4.3+ */
	icSigScreeningDescTag                  = 0x73637264,  /* 'scrd' V2.0 - V4.0 */
	icSigScreeningTag                      = 0x7363726E,  /* 'scrn' V2.0 - V4.0 */
	icSigTechnologyTag                     = 0x74656368,  /* 'tech' */
	icSigUcrBgTag                          = 0x62666420,  /* 'bfd ' V2.0 - V4.0 */
	icSigVideoCardGammaTag                 = 0x76636774,  /* 'vcgt' ColorSync 2.5+ */
	icSigViewingCondDescTag                = 0x76756564,  /* 'vued' */
	icSigViewingConditionsTag              = 0x76696577,  /* 'view' */
	icMaxEnumTag                           = icMaxTagVal 
} icTagSignature;

/* Aliases for backwards compatibility */
#define icSigBlueColorantTag  icSigBlueMatrixColumnTag		/* V2.0 - V2.4 */
#define icSigGreenColorantTag icSigGreenMatrixColumnTag		/* V2.0 - V2.4 */
#define icSigRedColorantTag   icSigRedMatrixColumnTag		/* V2.0 - V2.4 */

/* tag type signatures (+ version notes) */
typedef enum {
    icSigChromaticityType               = 0x6368726D,  /* 'chrm' V2.3+ */ 
	icSigCicpType                       = 0x63696370,  /* 'cicp' V4.4+ */ 
    icSigColorantOrderType              = 0x636C726F,  /* 'clro' V4.0+ */
    icSigColorantTableType              = 0x636C7274,  /* 'clrt' V4.0+ */
    icSigCrdInfoType                    = 0x63726469,  /* 'crdi' V2.1 - V4.0 */
    icSigCurveType                      = 0x63757276,  /* 'curv' */
    icSigDataType                       = 0x64617461,  /* 'data' */
    icSigDateTimeType                   = 0x6474696D,  /* 'dtim' */
    icSigDictType                       = 0x27646963,  /* 'dict' V4.4+ */
    icSigDeviceSettingsType             = 0x64657673,  /* 'devs' V2.2 - V4.0 */
    icSigLut16Type                      = 0x6d667432,  /* 'mft2' */
    icSigLut8Type                       = 0x6d667431,  /* 'mft1' */
    icSigLutAToBType                    = 0x6d414220,  /* 'mAB ' V4.0+ */
    icSigLutBToAType                    = 0x6d424120,  /* 'mBA ' V4.0+ */
    icSigMeasurementType                = 0x6D656173,  /* 'meas' */
    icSigMultiLocalizedUnicodeType      = 0x6D6C7563,  /* 'mluc' V4.0+ */
    icSigMultiProcessElementsType       = 0x6D706574,  /* 'mpet' V4.3+ */
    icSigNamedColorType                 = 0x6E636f6C,  /* 'ncol' V2.0 - V2.4 (deprecated) */
    icSigNamedColor2Type                = 0x6E636C32,  /* 'ncl2' V2.0+ */
    icSigParametricCurveType            = 0x70617261,  /* 'para' V4.0+ */
    icSigProfileSequenceDescType        = 0x70736571,  /* 'pseq' */
    icSigProfileSequenceIdentifierType  = 0x70736964,  /* 'psid' V4.3+ */
    icSigResponseCurveSet16Type         = 0x72637332,  /* 'rcs2' V2.2+ */
    icSigS15Fixed16ArrayType            = 0x73663332,  /* 'sf32' */
    icSigScreeningType                  = 0x7363726E,  /* 'scrn' V2.0 - V4.0 */
    icSigSignatureType                  = 0x73696720,  /* 'sig ' */
    icSigTextType                       = 0x74657874,  /* 'text' */
    icSigTextDescriptionType            = 0x64657363,  /* 'desc' V2.0 - V2.4 */
    icSigU16Fixed16ArrayType            = 0x75663332,  /* 'uf32' */
    icSigUcrBgType                      = 0x62666420,  /* 'bfd ' V2.0 - V4.0 */
    icSigUInt16ArrayType                = 0x75693136,  /* 'ui16' */
    icSigUInt32ArrayType                = 0x75693332,  /* 'ui32' */
    icSigUInt64ArrayType                = 0x75693634,  /* 'ui64' */
    icSigUInt8ArrayType                 = 0x75693038,  /* 'ui08' */
    icSigVideoCardGammaType             = 0x76636774,  /* 'vcgt' ColorSync 2.5+ */
    icSigViewingConditionsType          = 0x76696577,  /* 'view' */
    icSigXYZType                        = 0x58595A20,  /* 'XYZ ' */
    icMaxEnumType                       = icMaxTagVal   
} icTagTypeSignature;

/* Aliases */
#define icSigXYZArrayType   icSigXYZType

/* 
 * Color Space Signatures
 * Note that only icSigXYZData and icSigLabData are valid
 * Profile Connection Spaces (PCSs)
 */ 
typedef enum {
	/* PCS & Non-Dev */
    icSigXYZData                        = 0x58595A20,  /* 'XYZ ' */
    icSigLabData                        = 0x4C616220,  /* 'Lab ' */

	/* Non-Dev */
    icSigLuvData                        = 0x4C757620,  /* 'Luv ' */
    icSigYxyData                        = 0x59787920,  /* 'Yxy ' */

	/* Dev (== N-component) */
    icSigYCbCrData                      = 0x59436272,  /* 'YCbr' */
    icSigRgbData                        = 0x52474220,  /* 'RGB ' */
    icSigGrayData                       = 0x47524159,  /* 'GRAY' */
    icSigHsvData                        = 0x48535620,  /* 'HSV ' */
    icSigHlsData                        = 0x484C5320,  /* 'HLS ' */
    icSigCmykData                       = 0x434D594B,  /* 'CMYK' */
    icSigCmyData                        = 0x434D5920,  /* 'CMY ' */

	/* Dev (== N-component) & N-Color */
    icSig2colorData                     = 0x32434C52,  /* '2CLR' V2.1+ */
    icSig3colorData                     = 0x33434C52,  /* '3CLR' V2.1+ */
    icSig4colorData                     = 0x34434C52,  /* '4CLR' V2.1+ */
    icSig5colorData                     = 0x35434C52,  /* '5CLR' V2.1+ */
    icSig6colorData                     = 0x36434C52,  /* '6CLR' V2.1+ */
    icSig7colorData                     = 0x37434C52,  /* '7CLR' V2.1+ */
    icSig8colorData                     = 0x38434C52,  /* '8CLR' V2.1+ */
    icSig9colorData                     = 0x39434C52,  /* '9CLR' V2.1+ */
    icSig10colorData                    = 0x41434C52,  /* 'ACLR' V2.1+ */
    icSig11colorData                    = 0x42434C52,  /* 'BCLR' V2.1+ */
    icSig12colorData                    = 0x43434C52,  /* 'CCLR' V2.1+ */
    icSig13colorData                    = 0x44434C52,  /* 'DCLR' V2.1+ */
    icSig14colorData                    = 0x45434C52,  /* 'ECLR' V2.1+ */
    icSig15colorData                    = 0x46434C52,  /* 'FCLR' V2.1+ */

	/* ICCLIB extension (synonyms for N-Color) */
    icSigMch2Data                       = 0x4D434832,  /* 'MCH2' Colorsync ? */
    icSigMch3Data                       = 0x4D434833,  /* 'MCH3' Colorsync ? */
    icSigMch4Data                       = 0x4D434834,  /* 'MCH4' Colorsync ? */
    icSigMch5Data                       = 0x4D434835,  /* 'MCH5' Colorsync ? */
    icSigMch6Data                       = 0x4D434836,  /* 'MCH6' Colorsync ? Hexachrome: CMYKOG */
    icSigMch7Data                       = 0x4D434837,  /* 'MCH7' Colorsync ? */
    icSigMch8Data                       = 0x4D434838,  /* 'MCH8' Colorsync ? */
    icSigMch9Data                       = 0x4D434839,  /* 'MCH9' Colorsync ? */
    icSigMchAData                       = 0x4D434841,  /* 'MCHA' Colorsync ? */
    icSigMchBData                       = 0x4D434842,  /* 'MCHB' Colorsync ? */
    icSigMchCData                       = 0x4D434843,  /* 'MCHC' Colorsync ? */
    icSigMchDData                       = 0x4D434844,  /* 'MCHD' Colorsync ? */
    icSigMchEData                       = 0x4D434845,  /* 'MCHE' Colorsync ? */
    icSigMchFData                       = 0x4D434846,  /* 'MCHF' Colorsync ? */

    icMaxEnumData                       = icMaxTagVal   
} icColorSpaceSignature;

/* profileClass enumerations */
typedef enum {
    icSigInputClass                     = 0x73636E72,  /* 'scnr' */
    icSigDisplayClass                   = 0x6D6E7472,  /* 'mntr' */
    icSigOutputClass                    = 0x70727472,  /* 'prtr' */
    icSigLinkClass                      = 0x6C696E6B,  /* 'link' */
    icSigColorSpaceClass                = 0x73706163,  /* 'spac' */
    icSigAbstractClass                  = 0x61627374,  /* 'abst' */
    icSigNamedColorClass                = 0x6e6d636c,  /* 'nmcl' */
    icMaxEnumClass                      = icMaxTagVal  
} icProfileClassSignature;

/* Platform Signatures */
typedef enum {
    icSigPlatformUnknown                = 0x00000000,  /* */
    icSigMacintosh                      = 0x4150504C,  /* 'APPL' */
    icSigMicrosoft                      = 0x4D534654,  /* 'MSFT' */
    icSigSolaris                        = 0x53554E57,  /* 'SUNW' */
    icSigSGI                            = 0x53474920,  /* 'SGI ' */
    icSigTaligent                       = 0x54474E54,  /* 'TGNT' */
    icMaxEnumPlatform                   = icMaxTagVal  
} icPlatformSignature;

/* Device Manufacturer signatures */
/* (Omitted due to difficulty in getting a machine readable list) */
typedef unsigned int icDeviceManufacturerSignature;
#define icMaxEnumDeviceManufacturer (icDeviceManufacturerSignature)icMaxTagVal)  

/* Device Model signatures */
/* (Omitted due to difficulty in getting a machine readable list) */
typedef unsigned int icDeviceModelSignature;
#define icMaxEnumDeviceModel (icDeviceModelSignature)icMaxTagVal)  

/* CMM signatures (from registry, March 4 2021) */
typedef enum {
	icSigAdobeCMM                          = 0x41444245,  /* 'ADBE' */ 
	icSigAgfaCMM                           = 0x41434D53,  /* 'ACMS' */ 
	icSigAppleCMM                          = 0x6170706C,  /* 'appl' */ 
	icSigColorGearCMM                      = 0x43434D53,  /* 'CCMS' (Canon) */ 
	icSigColorGearLiteCMM                  = 0x5543434D,  /* 'UCCM' (Canon) */ 
	icSigColorGearCCMM                     = 0x55434D53,  /* 'UCMS' (Canon) */ 
	icSigEFICMM                            = 0x45464920,  /* 'EFI ' */ 
	icSigFujiFilmCMM                       = 0x46462020,  /* 'FF  ' */ 
	icSigExactScanCMM                      = 0x45584143,  /* 'EXAC' */ 
	icSigHarlequinCMM                      = 0x48434d4d,  /* 'HCMM' */ 
	icSigArgyllCMM                         = 0x6172676C,  /* 'argl' */ 
	icSigLogoSyncCMM                       = 0x44676f53,  /* 'LgoS' (GretagMacbeth) */ 
	icSigHeidelbergCMM                     = 0x48444d20,  /* 'HDM ' */ 
	icSigLittleCMSCMM                      = 0x6C636d73,  /* 'lcms' */ 
	icSigReflccMAXCMM                      = 0x52494d58,  /* 'RIMX' */ 
	icSigDemoIccMAXCMM                     = 0x44494d58,  /* 'DIMX' */ 
	icSigKodakCMM                          = 0x4b434d53,  /* 'KCMS' */ 
	icSigKonicaMinoltaCMM                  = 0x4d434d44,  /* 'MCML' */ 
	icSigMicrosoftCMM                      = 0x57435320,  /* 'WCS ' */ 
	icSigMutohCMM                          = 0x5349474E,  /* 'SIGN' */ 
	icSigOnyxGraphicsCMM                   = 0x4f4e5958,  /* 'ONYX' */ 
	icSigDeviceLinkCMM                     = 0x52474d53,  /* 'RGMS' (Rolf Gierling Multitools) */ 
	icSigSampleICCCMM                      = 0x53494343,  /* 'SICC' */ 
	icSigToshibaCMM                        = 0x54434d4d,  /* 'TCMM' */ 
	icSigImagingFactoryCMM                 = 0x33324254,  /* '32BT' */ 
	icSigVivoCMM                           = 0x7669766f,  /* 'vivo' */ 
	icSigWareToGoCMM                       = 0x57544720,  /* 'WTG ' */ 
	icSigZoranCMM                          = 0x7a633030,  /* 'zc00' */ 
	icMaxEnumCMM                           = icMaxTagVal 
} icCMMSignature;

/* Reference Medium Gamut Signatures */
typedef enum {
    icSigPerceptualReferenceMediumGamut = 0x70726d67,  /* 'prmg' V4.3+ */
    icMaxEnumReferenceMediumGamut       = icMaxTagVal  
} icReferenceMediumGamutSignature;

/* Technology signature descriptions */
typedef enum {
    icSigFilmScanner                    = 0x6673636E,  /* 'fscn' */
    icSigDigitalCamera                  = 0x6463616D,  /* 'dcam' */
    icSigReflectiveScanner              = 0x7273636E,  /* 'rscn' */
    icSigInkJetPrinter                  = 0x696A6574,  /* 'ijet' */ 
    icSigThermalWaxPrinter              = 0x74776178,  /* 'twax' */
    icSigElectrophotographicPrinter     = 0x6570686F,  /* 'epho' */
    icSigElectrostaticPrinter           = 0x65737461,  /* 'esta' */
    icSigDyeSublimationPrinter          = 0x64737562,  /* 'dsub' */
    icSigPhotographicPaperPrinter       = 0x7270686F,  /* 'rpho' */
    icSigFilmWriter                     = 0x6670726E,  /* 'fprn' */
    icSigVideoMonitor                   = 0x7669646D,  /* 'vidm' */
    icSigVideoCamera                    = 0x76696463,  /* 'vidc' */
    icSigProjectionTelevision           = 0x706A7476,  /* 'pjtv' */
    icSigCRTDisplay                     = 0x43525420,  /* 'CRT ' */
    icSigPMDisplay                      = 0x504D4420,  /* 'PMD ' */
    icSigAMDisplay                      = 0x414D4420,  /* 'AMD ' */
    icSigPhotoCD                        = 0x4B504344,  /* 'KPCD' */
    icSigPhotoImageSetter               = 0x696D6773,  /* 'imgs' */
    icSigGravure                        = 0x67726176,  /* 'grav' */
    icSigOffsetLithography              = 0x6F666673,  /* 'offs' */
    icSigSilkscreen                     = 0x73696C6B,  /* 'silk' */
    icSigFlexography                    = 0x666C6578,  /* 'flex' */
    icSigMotionPictureFilmScanner       = 0x6D706673,  /* 'mpfs' V4.3+ */
    icSigMotionPictureFilmRecorder      = 0x6D706673,  /* 'mpfr' V4.3+ */
    icSigDigitalMotionPictureCamera     = 0x646D7063,  /* 'dmpc' V4.3+ */
    icSigDigitalCinemaProjector         = 0x64636A70,  /* 'dcpj' V4.3+ */
    icMaxEnumTechnology                 = icMaxTagVal   
} icTechnologySignature;

/*------------------------------------------------------------------------*/
/*
 * Other enums (Should these be "icSig..." ?)
 */

/* Measurement Geometry, used in the measurmentType tag */
typedef enum {
    icGeometryUnknown                   = 0x00000000,  /* Unknown */
    icGeometry045or450                  = 0x00000001,  /* 0/45, 45/0 */
    icGeometry0dord0                    = 0x00000002,  /* 0/d or d/0 */
    icMaxEnumGeometry                   = icMaxTagVal   
} icMeasurementGeometry;

/* Measurement Flare, used in the measurmentType tag */
typedef enum {
    icFlare0                            = 0x00000000,  /* 0% flare */
    icFlare100                          = 0x00000001,  /* 100% flare */
    icMaxEnumFlare                      = icMaxTagVal   
} icMeasurementFlare;

/* Rendering Intents, used in the profile header */
typedef enum {
    icPerceptual                        = 0,
    icRelativeColorimetric              = 1,
    icSaturation                        = 2,
    icAbsoluteColorimetric              = 3,
    icMaxEnumIntent                     = icMaxTagVal   
} icRenderingIntent;

/* Different Spot Shapes currently defined, used for screeningType */
typedef enum {
    icSpotShapeUnknown                  = 0,
    icSpotShapePrinterDefault           = 1,
    icSpotShapeRound                    = 2,
    icSpotShapeDiamond                  = 3,
    icSpotShapeEllipse                  = 4,
    icSpotShapeLine                     = 5,
    icSpotShapeSquare                   = 6,
    icSpotShapeCross                    = 7,
    icMaxEnumSpot                       = icMaxTagVal   
} icSpotShape;

/* Standard Observer, used in the measurmentType tag */
typedef enum {
    icStdObsUnknown                     = 0x00000000,  /* Unknown */
    icStdObs1931TwoDegrees              = 0x00000001,  /* 2 deg */
    icStdObs1964TenDegrees              = 0x00000002,  /* 10 deg */
    icMaxEnumStdObs                     = icMaxTagVal   
} icStandardObserver;

/* Pre-defined illuminants, used in measurement and viewing conditions type */
typedef enum {
    icIlluminantUnknown                 = 0x00000000,
    icIlluminantD50                     = 0x00000001,
    icIlluminantD65                     = 0x00000002,
    icIlluminantD93                     = 0x00000003,
    icIlluminantF2                      = 0x00000004,
    icIlluminantD55                     = 0x00000005,
    icIlluminantA                       = 0x00000006,
    icIlluminantEquiPowerE              = 0x00000007, 
    icIlluminantF8                      = 0x00000008,
    icMaxEnumIlluminant                 = icMaxTagVal   
} icIlluminant;

/* A not so exhaustive list of language codes */
typedef enum {
	icLanguageCodeEnglish               = 0x656E, /* 'en' */
	icLanguageCodeGerman                = 0x6465, /* 'de' */
	icLanguageCodeItalian               = 0x6974, /* 'it' */
	icLanguageCodeDutch                 = 0x6E6C, /* 'nl' */
	icLanguageCodeSweden                = 0x7376, /* 'sv' */
	icLanguageCodeSpanish               = 0x6573, /* 'es' */
	icLanguageCodeDanish                = 0x6461, /* 'da' */
	icLanguageCodeNorwegian             = 0x6E6F, /* 'no' */
	icLanguageCodeJapanese              = 0x6A61, /* 'ja' */
	icLanguageCodeFinish                = 0x6669, /* 'fi' */
	icLanguageCodeTurkish               = 0x7472, /* 'tr' */
	icLanguageCodeKorean                = 0x6B6F, /* 'ko' */
	icLanguageCodeChinese               = 0x7A68, /* 'zh' */
	icLanguageCodeFrench                = 0x6672, /* 'fr' */
	icMaxEnumLanguageCode               = icMaxTagVal
} icEnumLanguageCode;

/* A not so exhaustive list of region codes. */
typedef enum {
	icRegionCodeUSA                      = 0x5553, /* 'US' */
	icRegionCodeUnitedKingdom            = 0x554B, /* 'UK' */
	icRegionCodeGermany                  = 0x4445, /* 'DE' */
	icRegionCodeItaly                    = 0x4954, /* 'IT' */
	icRegionCodeNetherlands              = 0x4E4C, /* 'NL' */
	icRegionCodeSpain                    = 0x4543, /* 'ES' */
	icRegionCodeDenmark                  = 0x444B, /* 'DK' */
	icRegionCodeNorway                   = 0x4E4F, /* 'ND' */
	icRegionCodeJapan                    = 0x4A50, /* 'JP' */
	icRegionCodeFinland                  = 0x4649, /* 'FI' */
	icRegionCodeTurkey                   = 0x5452, /* 'TR' */
	icRegionCodeKorea                    = 0x4B52, /* 'KR' */
	icRegionCodeChina                    = 0x434E, /* 'CN' */
	icRegionCodeTaiwan                   = 0x5457, /* 'TW' */
	icRegionCodeFrance                   = 0x4652, /* 'FR' */
	icRegionCodeAustralia                = 0x4155, /* 'AU' */
	icMaxEnumRegionCode                  = icMaxTagVal
} icEnumRegionCode;

/* Microsoft platform Device Settings ID Signatures */
typedef enum {
    icSigMsftResolution                  = 0x72736c6e, /* 'rsln' */
    icSigMsftMedia                       = 0x6d747970, /* 'mtyp' */
    icSigMsftHalftone                    = 0x6866746e, /* 'hftn' */
    icMaxEnumDevSetMsftID                = icMaxTagVal
} icDevSetMsftIDSignature;

/* Microsoft platform Media Type Encoding */
typedef enum {
    icMsftMediaStandard                  = 1,
    icMsftMediaTrans                     = 2, /* transparency */
    icMsftMediaGloss                     = 3,
    icMsftMediaUser1                     = 256,
    icMaxEnumMsftMedia                   = icMaxTagVal
} icDevSetMsftMedia;

/* Microsoft platform Halftone Values */
typedef enum {
    icMsftDitherNone                     = 1,
    icMsftDitherCoarse                   = 2,
    icMsftDitherFine                     = 3,
    icMsftDitherLineArt                  = 4,
    icMsftDitherErrorDiffusion           = 5,
    icMsftDitherReserved6                = 6,
    icMsftDitherReserved7                = 7,
    icMsftDitherReserved8                = 8,
    icMsftDitherReserved9                = 9,
    icMsftDitherGrayScale                = 10,
    icMsftDitherUser1                    = 256,
    icMsftMaxEnumMsftDither              = icMaxTagVal
} icDevSetMsftDither;

/* measurement units for the icResponseCurveSet16Type */
typedef enum {
    icSigStatusA                         = 0x53746141, /* 'StaA' */
    icSigStatusE                         = 0x53746145, /* 'StaE' */
    icSigStatusI                         = 0x53746149, /* 'StaI' */
    icSigStatusT                         = 0x53746154, /* 'StaT' */
    icSigStatusM                         = 0x5374614d, /* 'StaM' */
    icSigDN                              = 0x444e2020, /* 'DN ' */
    icSigDNP                             = 0x444e2050, /* 'DN P' */
    icSigDNN                             = 0x444e4e20, /* 'DNN ' */
    icSigDNNP                            = 0x444e4e50, /* 'DNNP' */
    icMaxEnumMeasUnits                   = icMaxTagVal
} icMeasUnitsSig;

/* Phosphor Colorant and Phosphor Encodings used in chromaticity type */
typedef enum {
	icPhColUnknown                   = 0x0000, /* Unknown */
	icPhColITU_R_BT_709              = 0x0001, /* ITU-R BT.709 */
	icPhColSMPTE_RP145_1994          = 0x0002, /* SMPTE RP145-1994 */
	icPhColEBU_Tech_3213_E           = 0x0003, /* EBU Tech.3213-E */
	icPhColP22                       = 0x0004, /* P22 */
	icPhColP3                        = 0x0005, /* P3 */
	icPhColITU_R_BT2020              = 0x0006, /* ITU-R BT.2020 */
	icMaxEnumPhCol                   = icMaxTagVal
} icPhColEncoding;

/* icSigMultiProcessElementsType elements */
typedef enum {
    icSigCurveSetElement               = 0x6D666C74,  /* 'cvst' */ 
    icSigOneDCurveEncoding             = 0x63757266,  /* 'curf' */ 
    icSigFormulaCurveSegEncoding       = 0x70617266,  /* 'parf' */ 
    icSigSampledCurveSegEncoding       = 0x73616D66,  /* 'samf' */ 
    icSigMatrixElement                 = 0x6D617466,  /* 'matf' */ 
    icSigCLUTElement                   = 0x636C7574,  /* 'clut' */ 
    icSigBACSElement                   = 0x62414353,  /* 'bACS' */ 
    icSigEACSElement                   = 0x65414353,  /* 'eACS’' */ 
    icMaxEnumMPElement                 = icMaxTagVal
} icMultiProcessElementSig;

/* Formula curve segment encoding */
typedef enum {
    icCurveSegFunction1        = 0x0000, /* Y = (a * X + b)^l + c */
    icCurveSegFunction2        = 0x0001, /* Y = a * log10(b * X^l + c) + d */
    icCurveSegFunction3        = 0x0002, /* Y = a * b^(c * X + d) + e */
    icMaxEnumCurveSegFunctionType = icMaxTagVal
} icCurveSegFunctionType;


/* Parametric curve types for icSigParametricCurveType */
typedef enum {
    icCurveFunction1        = 0x0000, /* Y = X ^ p0 */
    icCurveFunction2        = 0x0001, /* Y = X >= -p2/p1 ? Y = (p1 * X + p2)^p0 : 0 */
    icCurveFunction3        = 0x0002, /* Y = X >= -p2/p1 ? Y = (p1 * X + p2)^p0 + p3 : p3  */
    icCurveFunction4        = 0x0003, /* Y = X >= p4 ? Y = (p1 * X + p2)^p0 : p3 * X */
    icCurveFunction5        = 0x0004, /* Y = X >= p4 ? Y = (p1 * X + p2)^p0 + p5 : p3 * X + p6 */
    icMaxEnumCurveFunctionType   = icMaxTagVal
} icParametricCurveFunctionType;

/* Types for icSigColorimetricIntentImageStateTag signatures (V4.3+) */
typedef enum {
    icSceneColorimetryEstimates         	= 0x73636F65, /* 'scoe' */
	icSceneAppearanceEstimates				= 0x73617065, /* 'sape' */
	icFocalPlaneColorimetryEstimates		= 0x66706365, /* 'fpce' */
	icReflectionHardcopyOriginalColorimetry	= 0x72686F63, /* 'rhoc' */
	icReflectionPrintOutputColorimetry		= 0x72706F63, /* 'rpoc' */
    icMaxEnumColorimetricIntentImageState   = icMaxTagVal
} icColorimetricIntentImageStateType;

/* Types for icSigPerceptualRenderingIntentGamutTag (V4.3+) */
/* and icSigSaturationRenderingIntentGamutTag */
typedef enum {
    icPerceptualReferenceMediumGamut		= 0x70726D67, /* 'prmg' */
	icMaxEnumRenderingIntentGamut           = icMaxTagVal
} icSigRenderingIntentGamutType;


#ifdef __cplusplus
	}
#endif

#endif /* ICCV44_H */
