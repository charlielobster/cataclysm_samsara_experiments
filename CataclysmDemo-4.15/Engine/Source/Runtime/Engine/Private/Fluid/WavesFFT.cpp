// CATACLYSM 
#include "WavesFFT.h"
#include "EnginePrivatePCH.h"
#include "FluidSimulation.h"
#include "FluidGlobalShaders.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FPrepareFFTUniformParameters, TEXT("PrepareFFTParams"));
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FExecuteFFTUniformParameters, TEXT("ExecuteFFTParams"));

IMPLEMENT_SHADER_TYPE(, FPrepareFFTCS, TEXT("FluidWavesFFT"), TEXT("PrepareFFT"), SF_Compute);
IMPLEMENT_SHADER_TYPE(, FExecuteFFTCS, TEXT("FluidWavesFFT"), TEXT("ExecuteFFT"), SF_Compute);

FWavesFFT::FWavesFFT(uint32 InSize, float VoxelWidth, float WavesLengthPeriod, float WavesMaxWaveLength, float WavesWindSpeed)
{
	Size = InSize;
	SizeBits = FMath::FloorLog2(Size);

	FRHIResourceCreateInfo CreateInfo;
	SpectrumTexture = RHICreateTexture2D(Size, Size, PF_A32B32G32R32F, 1, 1, TexCreate_ShaderResource, CreateInfo);

	for (int i = 0; i < 2; ++i)
	{
		TextureFFT[i] = RHICreateTexture2D(Size, Size, PF_A32B32G32R32F, 1, 1, TexCreate_UAV | TexCreate_ShaderResource, CreateInfo);
		TextureFFT_UAV[i] = RHICreateUnorderedAccessView(TextureFFT[i]);
	}

	GenerateSpectrum(VoxelWidth, WavesLengthPeriod, WavesMaxWaveLength, WavesWindSpeed);
}

FWavesFFT::~FWavesFFT()
{
}

void FWavesFFT::Destroy()
{
	for (int i = 0; i < 2; ++i)
	{
		TextureFFT_UAV[i].SafeRelease();
		TextureFFT[i].SafeRelease();
	}
	SpectrumTexture.SafeRelease();

	delete this;
}

FTexture2DRHIRef FWavesFFT::Update(FRHICommandListImmediate& RHICmdList, float Time)
{
	TShaderMapRef<FPrepareFFTCS> PrepareFFTCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	{
		FPrepareFFTUniformParameters PrepareFFTParams;
		PrepareFFTParams.Bits = SizeBits;
		PrepareFFTParams.Step = GridStep;
		PrepareFFTParams.Time = Time;

		FLocalUniformBuffer LocalUB = TUniformBufferRef<FPrepareFFTUniformParameters>::CreateLocalUniformBuffer(RHICmdList, PrepareFFTParams, UniformBuffer_SingleDraw);

		RHICmdList.SetComputeShader(PrepareFFTCS->GetComputeShader());

		PrepareFFTCS->SetParameters(RHICmdList, SpectrumTexture, LocalUB);
		PrepareFFTCS->SetOutput(RHICmdList, TextureFFT_UAV[0]);

		DispatchComputeShader(RHICmdList, *PrepareFFTCS, Size / 32, Size / 32, 1);
		PrepareFFTCS->UnbindBuffers(RHICmdList);
	}

	uint32 Index = 0;

	TShaderMapRef<FExecuteFFTCS> ExecuteFFTCS(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	for (uint32 Bit = 0; Bit < SizeBits; ++Bit)
	{
		FExecuteFFTUniformParameters ExecuteFFTParams;
		ExecuteFFTParams.Bits = SizeBits;
		ExecuteFFTParams.Bit = Bit;

		FLocalUniformBuffer LocalUB = TUniformBufferRef<FExecuteFFTUniformParameters>::CreateLocalUniformBuffer(RHICmdList, ExecuteFFTParams, UniformBuffer_SingleDraw);

		RHICmdList.SetComputeShader(ExecuteFFTCS->GetComputeShader());

		ExecuteFFTCS->SetParameters(RHICmdList, TextureFFT[Index], LocalUB);
		ExecuteFFTCS->SetOutput(RHICmdList, TextureFFT_UAV[Index ^ 1]);

		DispatchComputeShader(RHICmdList, *ExecuteFFTCS, Size / 32, Size / 32, 1);
		ExecuteFFTCS->UnbindBuffers(RHICmdList);

		Index = Index ^ 1;
	}
	return TextureFFT[Index];
}

static double sp_gamma(double z)
{
	const int a = 12;
	static double c_space[a];
	static double *c = NULL;
	int k;
	double accm;

	if (c == NULL) {
		double k1_factrl = 1.0; /* (k - 1)!*(-1)^k with 0!==1*/
		c = c_space;
		c[0] = sqrt(2.0*PI);
		for (k = 1; k < a; k++) {
			c[k] = exp(double(a - k)) * pow(a - k, k - 0.5) / k1_factrl;
			k1_factrl *= -k;
		}
	}
	accm = c[0];
	for (k = 1; k < a; k++) {
		accm += c[k] / (z + k);
	}
	accm *= exp(-(z + a)) * pow(z + a, z + 0.5); /* Gamma(z+1) */
	return accm / z;
}

class MTRand {
	// Data
public:
	typedef unsigned long uint32;  // unsigned integer type, at least 32 bits

	enum { N = 624 };       // length of state vector
	enum { SAVE = N + 1 };  // length of array for save()

protected:
	enum { M = 397 };  // period parameter

	uint32 state[N];   // internal state
	uint32 *pNext;     // next value to get from state
	int left;          // number of values left before reload needed


   //Methods
public:
	MTRand(const uint32& oneSeed)
	{
		seed(oneSeed);
	}

	// Access to 32-bit random numbers
	double rand() // real number in [0,1]
	{
		return double(randInt()) * (1.0 / 4294967295.0);
	}
	double randExc() // real number in [0,1)
	{
		return double(randInt()) * (1.0 / 4294967296.0);
	}
	double randDblExc() // real number in (0,1)
	{
		return (double(randInt()) + 0.5) * (1.0 / 4294967296.0);
	}

	uint32 randInt() // integer in [0,2^32-1]
	{
		// Pull a 32-bit integer from the generator state
		// Every other access function simply transforms the numbers extracted here

		if (left == 0) reload();
		--left;

		register uint32 s1;
		s1 = *pNext++;
		s1 ^= (s1 >> 11);
		s1 ^= (s1 << 7) & 0x9d2c5680UL;
		s1 ^= (s1 << 15) & 0xefc60000UL;
		return (s1 ^ (s1 >> 18));
	}

	// Re-seeding functions with same behavior as initializers
	void seed(const uint32 oneSeed)
	{
		// Seed the generator with a simple uint32
		initialize(oneSeed);
		reload();
	}

protected:
	void initialize(const uint32 oneSeed)
	{
		// Initialize generator state with seed
		// See Knuth TAOCP Vol 2, 3rd Ed, p.106 for multiplier.
		// In previous versions, most significant bits (MSBs) of the seed affect
		// only MSBs of the state array.  Modified 9 Jan 2002 by Makoto Matsumoto.
		register uint32 *s = state;
		register uint32 *r = state;
		register int i = 1;
		*s++ = oneSeed & 0xffffffffUL;
		for (; i < N; ++i)
		{
			*s++ = (1812433253UL * (*r ^ (*r >> 30)) + i) & 0xffffffffUL;
			r++;
		}
	}

	void reload()
	{
		// Generate N new values in state
		// Made clearer and faster by Matthew Bellew (matthew.bellew@home.com)
		register uint32 *p = state;
		register int i;
		for (i = N - M; i--; ++p)
			*p = twist(p[M], p[0], p[1]);
		for (i = M; --i; ++p)
			*p = twist(p[M - N], p[0], p[1]);
		*p = twist(p[M - N], p[0], state[0]);

		left = N, pNext = state;
	}

	uint32 hiBit(const uint32& u) const { return u & 0x80000000UL; }
	uint32 loBit(const uint32& u) const { return u & 0x00000001UL; }
	uint32 loBits(const uint32& u) const { return u & 0x7fffffffUL; }
	uint32 mixBits(const uint32& u, const uint32& v) const
	{
		return hiBit(u) | loBits(v);
	}
	uint32 twist(const uint32& m, const uint32& s0, const uint32& s1) const
	{
		return m ^ (mixBits(s0, s1) >> 1) ^ ((~loBit(s1) + 1) & 0x9908b0dfUL);
	}
};


void FWavesFFT::GenerateSpectrum(float VoxelWidth, float WavesLengthPeriod, float WavesMaxWaveLength, float WavesWindSpeed)
{
	const float LengthPeriod = WavesLengthPeriod * 0.01f;
	const float MaxWaveLength = WavesMaxWaveLength * 0.01f;
	GridStep = LengthPeriod / Size;

	struct Complex
	{
		float Re, Im;

		//static Complex Zero;

		Complex() {}
		explicit Complex(float re, float im) : Re(re), Im(im) {}
	};

	MTRand rand(0x1234567);

	const float _wsAmplitude = 1.0f; // just scale
	const float _wsGravity = 9.8f; // m/sec^2
	const float _wsWindVel = WavesWindSpeed; // m/sec
	const float _wsWindDir[2] = { 1.0f, 0.0f };

	const float SpectrumCutoff = 0.95f;


	float kMax = (2 * PI) / (2 * GridStep);
	kMax *= SpectrumCutoff;
	float kMin = (2 * PI) / MaxWaveLength;

	const uint32 SuperSampling = 4; //4x supersampling
	const uint32 ssize = Size * SuperSampling;

	const float kStep = (2 * PI) / (GridStep * ssize);

	const float omegaPeak = (0.855f * _wsGravity) / _wsWindVel;

	float* energySpectrum = new float[ssize * ssize];
	for (uint32 y = 0; y < ssize; ++y)
	{
		for (uint32 x = 0; x < ssize; ++x)
		{
			float kx = kStep * (float(int(x) - int(ssize >> 1)) + 0.5f);
			float ky = kStep * (float(int(y) - int(ssize >> 1)) + 0.5f);

			float k = sqrtf(kx*kx + ky*ky);
			float E = 0;
			if (k >= kMin && k < kMax)
			{
				float cosTheta = (kx * _wsWindDir[0] + ky * _wsWindDir[1]) / k;

				//calculate amplitude from the spectrum
				float omega = sqrt(_wsGravity * k);

				float Sp = (0.0081f * _wsGravity * _wsGravity) / pow(omega, 5.0f);
				float Spm = Sp * exp(-(5.0f / 4) * powf(omegaPeak / omega, 4.0f));

				float s = 11.5f * pow(_wsGravity / (omegaPeak * _wsWindVel), 2.5f) * pow(omega / omegaPeak, omega > omegaPeak ? -2.5f : 5.0f);

				float cosHalfTheta = (1 + cosTheta)*0.5f;
				float D = pow(cosHalfTheta, 2 * s) * sp_gamma(s + 1) / (sp_gamma(s + 0.5f) * sqrt(PI) * 2);

				E = Spm * D * omega * kStep * kStep / (2 * k * k);

				check(_finite(E));
			}
			energySpectrum[x + y * ssize] = E;
		}
	}

	Complex* complexSpectrum[2];
	complexSpectrum[0] = new Complex[Size * Size];
	complexSpectrum[1] = new Complex[Size * Size];
	for (uint32 y = 0; y < Size; ++y)
	{
		for (uint32 x = 0; x < Size; ++x)
		{
			uint32 xss = x * SuperSampling;
			uint32 yss = y * SuperSampling;

			float energy = 0.0f;
			for (int sy = 0; sy < SuperSampling; ++sy)
			{
				for (int sx = 0; sx < SuperSampling; ++sx)
				{
					energy += energySpectrum[(xss + sx) + (yss + sy) * ssize];
				}
			}

			for (int i = 0; i < 2; ++i)
			{
				double phi = 2.0 * 3.14159265358979323846264338328 * rand.randExc();
				double Re = cos(phi), Im = sin(phi);
				double R = sqrt(-2.0 * log(rand.randDblExc()));
				float amplitude = R * sqrt(energy);

				complexSpectrum[i][x + y * Size] = Complex(amplitude * Re, amplitude * Im);
			}
		}
	}

	float* tangledData = new float[4 * Size * Size];
	for (int mip = 0; mip < 1; ++mip)
	{
		int mipSize = (Size >> mip);
		int mipBits = (SizeBits - mip);

		for (int y = 0; y < mipSize; ++y)
			for (int x = 0; x < mipSize; ++x)
			{
				int xs = ReverseBits(uint32(x)) >> (32 - mipBits);
				int ys = ReverseBits(uint32(y)) >> (32 - mipBits);

				xs -= ((xs << 1) & mipSize);
				ys -= ((ys << 1) & mipSize);

				xs += (Size >> 1);
				ys += (Size >> 1);
				Complex c0 = complexSpectrum[0][xs + ys * Size];
				Complex c1 = complexSpectrum[1][xs + ys * Size];

				tangledData[(x + y * Size) * 4 + 0] = c0.Re;
				tangledData[(x + y * Size) * 4 + 1] = c0.Im;
				tangledData[(x + y * Size) * 4 + 2] = c1.Re;
				tangledData[(x + y * Size) * 4 + 3] = c1.Im;
			}

		FUpdateTextureRegion2D Region(0, 0, 0, 0, Size, Size);
		RHIUpdateTexture2D(SpectrumTexture, 0, Region, Size * 4 * sizeof(float), (uint8*)tangledData);
	}
	delete[] tangledData;
	delete[] complexSpectrum[1];
	delete[] complexSpectrum[0];
	delete[] energySpectrum;
}
