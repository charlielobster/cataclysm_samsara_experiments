#pragma once
#include "Spectrum.h"

//Absorption coefficient of pure sea water
//360 to 780 step 10 nm
static double Asw_Spectrum[] = {
	0.0379, //360
	0.0300, //370
	0.0220, //380
	0.0191, //390
	0.0171, //400
	0.0162, //410
	0.0153, //420
	0.0144, //430
	0.0145, //440
	0.0145, //450
	0.0156, //460
	0.0156, //470
	0.0176, //480
	0.0196, //490
	0.0257, //500
	0.0357, //510
	0.0477, //520
	0.0507, //530
	0.0558, //540
	0.0638, //550
	0.0708, //560
	0.0799, //570
	0.108,  //580
	0.157,  //590
	0.244,  //600
	0.289,  //610
	0.309,  //620
	0.319,  //630
	0.329,  //640
	0.349,  //650
	0.400,  //660
	0.430,  //670
	0.450,  //680
	0.500,  //690
	0.650,  //700
	0.839,  //710
	1.169,  //720
	1.799,  //730
	2.38,   //740
	2.47,   //750
	2.55,   //760
	2.51,   //770
	2.36,   //780
};
RegSpectralCurve Asw_Curve(Asw_Spectrum, 360, 780, 10);

//Scattering coefficient of pure sea water
//360 to 780 step 10 nm
static double Bsw_Spectrum[] = {
	0.0120, //360
	0.0106, //370
	0.0094, //380
	0.0084, //390
	0.0076, //400
	0.0068, //410
	0.0061, //420
	0.0055, //430
	0.0049, //440
	0.0045, //450
	0.0041, //460
	0.0037, //470
	0.0034, //480
	0.0031, //490
	0.0029, //500
	0.0026, //510
	0.0024, //520
	0.0022, //530
	0.0021, //540
	0.0019, //550
	0.0018, //560
	0.0017, //570
	0.0016, //580
	0.0015, //590
	0.0014, //600
	0.0013, //610
	0.0012, //620
	0.0011, //630
	0.0010, //640
	0.0010, //650
	0.0008, //660
	0.0008, //670
	0.0007, //680
	0.0007, //690
	0.0007, //700
	0.0007, //710
	0.0006, //720
	0.0006, //730
	0.0006, //740
	0.0005, //750
	0.0005, //760
	0.0005, //770
	0.0004, //780
};
RegSpectralCurve Bsw_Curve(Bsw_Spectrum, 360, 780, 10);

//400 to 700 step 10 nm
static double Morel_Aw_Spectrum[] = {
	0.018, //400
	0.017, //410
	0.016, //420
	0.015, //430
	0.015, //440
	0.015, //450
	0.016, //460
	0.016, //470
	0.018, //480
	0.020, //490
	0.026, //500
	0.036, //510
	0.048, //520
	0.051, //530
	0.056, //540
	0.064, //550
	0.071, //560
	0.080, //570
	0.108, //580
	0.157, //590
	0.245, //600
	0.290, //610
	0.310, //620
	0.320, //630
	0.330, //640
	0.350, //650
	0.410, //660
	0.430, //670
	0.450, //680
	0.500, //690
	0.650, //700
};
RegSpectralCurve Morel_Aw_Curve(Morel_Aw_Spectrum, 400, 700, 10);

//400 to 700 step 10 nm
static double Morel_Ac_Spectrum[] = {
	0.687, //400
	0.828, //410
	0.913, //420
	0.973, //430
	1.000, //440
	0.944, //450
	0.917, //460
	0.870, //470
	0.798, //480
	0.750, //490
	0.668, //500
	0.618, //510
	0.528, //520
	0.474, //530
	0.416, //540
	0.357, //550
	0.294, //560
	0.276, //570
	0.291, //580
	0.282, //590
	0.236, //600
	0.252, //610
	0.276, //620
	0.317, //630
	0.334, //640
	0.356, //650
	0.441, //660
	0.595, //670
	0.502, //680
	0.329, //690
	0.215, //700
};
RegSpectralCurve Morel_Ac_Curve(Morel_Ac_Spectrum, 400, 700, 10);

class OceanSpectrum
{
public:
	static double ComputeMorelA(double lambda, double Cp)
	{
		return (Morel_Aw_Curve(lambda) + 0.06*Morel_Ac_Curve(lambda)*pow(Cp, 0.65))*(1 + 0.2*exp(-0.014*(lambda - 440)));
	}
	static double ComputeMorelB(double lambda, double Cp)
	{
		return Bsw_Curve(lambda) + (550 / lambda) * 0.30 * pow(Cp, 0.62);
	}
	static double ComputeMorelBb(double lambda, double Cp)
	{
		return 0.5*Bsw_Curve(lambda) + (0.002 + 0.02*(0.5 - 0.25*log10(Cp))*(550 / lambda))*(0.30*pow(Cp, 0.62) - Bsw_Curve(550));
	}
	static double ComputeMorelC(double lambda, double Cp)
	{
		return ComputeMorelA(lambda, Cp) + ComputeMorelB(lambda, Cp);
	}

	static double ComputeLogDistMultiplier(double Cp)
	{
		double cMin = FLT_MAX;
		for (int i = 0; i < VisSpectrum::LambdaCount; ++i)
		{
			// lambda in nm
			double lambda = VisSpectrum::GetLambda(i);
			if (lambda >= 400 && lambda <= 700)
			{
				cMin = min(ComputeMorelC(lambda, Cp), cMin);
			}
		}
		return 2 / cMin;
	}

	static Tuple3d ComputeWaterAttenuationColor(double dist, double Cp)
	{
		double spectrum[VisSpectrum::LambdaCount];
		for (int i = 0; i < VisSpectrum::LambdaCount; ++i)
		{
			// lambda in nm
			double lambda = VisSpectrum::GetLambda(i);

			double c = ComputeMorelC(lambda, Cp);
			spectrum[i] = exp(-c * dist);
		}
		Tuple3d colorXYZ = VisSpectrum::ComputeXYZ_D65(spectrum);
		Tuple3d colorRGB = XYZ_to_sRGB(colorXYZ);
		return colorRGB;
	}

	static Tuple3d ComputeWaterDiffuseColor(double dist, double cosTh, double Cp)
	{
		double spectrum[VisSpectrum::LambdaCount];
		for (int i = 0; i < VisSpectrum::LambdaCount; ++i)
		{
			// lambda in nm
			double lambda = VisSpectrum::GetLambda(i);
			if (lambda >= 400 && lambda <= 700)
			{
				double A = ComputeMorelA(lambda, Cp);
				double Bb = ComputeMorelBb(lambda, Cp);
				double C = A + ComputeMorelB(lambda, Cp);

				double S = 0.33 * Bb / A;
				spectrum[i] = S * (1 - exp(-(C + A*cosTh) * dist));
			}
			else
			{
				spectrum[i] = 0.0f;
			}
		}
		Tuple3d colorXYZ = VisSpectrum::ComputeXYZ_D65(spectrum);
		Tuple3d colorRGB = XYZ_to_sRGB(colorXYZ);
		return colorRGB;
	}

};
