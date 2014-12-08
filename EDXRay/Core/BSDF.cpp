#include "BSDF.h"
#include "../Reflection/Principled.h"
#include "Math/EDXMath.h"
#include "DifferentialGeom.h"
#include "Sampling.h"
#include "Sampler.h"

namespace EDX
{
	namespace RayTracer
	{
		// -----------------------------------------------------------------------------------------------------------------------
		// BSDF interface implementation
		// -----------------------------------------------------------------------------------------------------------------------
		BSDF* BSDF::CreateBSDF(const BSDFType type, const Color& color)
		{
			switch (type)
			{
			case BSDFType::Diffuse:
				return new LambertianDiffuse(color);
			case BSDFType::Mirror:
				return new Mirror(color);
			case BSDFType::Glass:
				return new Glass(color);
			case BSDFType::Principled:
				return new Principled(color, 0.1);
			}

			assert(0);
			return NULL;
		}
		BSDF* BSDF::CreateBSDF(const BSDFType type, const char* strTexPath)
		{
			switch (type)
			{
			case BSDFType::Diffuse:
				return new LambertianDiffuse(strTexPath);
			case BSDFType::Mirror:
				return new Mirror(strTexPath);
			case BSDFType::Glass:
				return new Glass(strTexPath);
			}

			assert(0);
			return NULL;
		}

		BSDF::BSDF(ScatterType t, BSDFType t2, const Color& color)
			: mScatterType(t), mBSDFType(t2)
		{
			mpTexture = new ConstantTexture2D<Color>(color);
		}
		BSDF::BSDF(ScatterType t, BSDFType t2, const char* pFile)
			: mScatterType(t), mBSDFType(t2)
		{
			mpTexture = new ImageTexture<Color, Color4b>(pFile);
		}


		Color BSDF::Eval(const Vector3& vOut, const Vector3& vIn, const DifferentialGeom& diffGoem, ScatterType types) const
		{
			if (Math::Dot(vOut, diffGoem.mGeomNormal) * Math::Dot(vIn, diffGoem.mGeomNormal) > 0.0f)
				types = ScatterType(types & ~BSDF_TRANSMISSION);
			else
				types = ScatterType(types & ~BSDF_REFLECTION);

			if (!MatchesTypes(types))
			{
				return Color::BLACK;
			}

			Vector3 vWo = diffGoem.WorldToLocal(vOut);
			Vector3 vWi = diffGoem.WorldToLocal(vIn);

			return GetColor(diffGoem) * Eval(vWo, vWi, types);
		}

		float BSDF::Pdf(const Vector3& vOut, const Vector3& vIn, const DifferentialGeom& diffGoem, ScatterType types /* = BSDF_ALL */) const
		{
			if (!MatchesTypes(types))
			{
				return 0.0f;
			}

			Vector3 vWo = diffGoem.WorldToLocal(vOut);
			Vector3 vWi = diffGoem.WorldToLocal(vIn);

			return Pdf(vWo, vWi, types);
		}

		float BSDF::Fresnel(float cosi, float etai, float etat)
		{
			cosi = Math::Clamp(cosi, -1.0f, 1.0f);

			bool entering = cosi > 0.0f;
			float ei = etai, et = etat;
			if (!entering)
				swap(ei, et);

			float sint = ei / et * Math::Sqrt(Math::Max(0.0f, 1.0f - cosi * cosi));

			if (sint >= 1.0f)
				return 1.0f;
			else
			{
				float cost = Math::Sqrt(Math::Max(0.0f, 1.0f - sint * sint));
				cosi = Math::Abs(cosi);

				float para = ((etat * cosi) - (etai * cost)) /
					((etat * cosi) + (etai * cost));
				float perp = ((etai * cosi) - (etat * cost)) /
					((etai * cosi) + (etat * cost));

				return (para * para + perp * perp) / 2.0f;
			}
		}

		// -----------------------------------------------------------------------------------------------------------------------
		// Lambertian BRDF Implementation
		// -----------------------------------------------------------------------------------------------------------------------
		float LambertianDiffuse::Eval(const Vector3& wo, const Vector3& wi, ScatterType types) const
		{
			if (!BSDFCoordinate::SameHemisphere(wo, wi))
				return 0.0f;

			return float(Math::EDX_INV_PI);
		}

		float LambertianDiffuse::Pdf(const Vector3& wo, const Vector3& wi, ScatterType types /* = BSDF_ALL */) const
		{
			if (!BSDFCoordinate::SameHemisphere(wo, wi))
				return 0.0f;

			return BSDFCoordinate::AbsCosTheta(wi) * float(Math::EDX_INV_PI);
		}

		Color LambertianDiffuse::SampleScattered(const Vector3& vOut, const Sample& sample, const DifferentialGeom& diffGoem, Vector3* pvIn, float* pPdf,
			ScatterType types, ScatterType* pSampledTypes) const
		{
			if (!MatchesTypes(types))
			{
				*pPdf = 0.0f;
				return Color::BLACK;
			}

			Vector3 vWo = diffGoem.WorldToLocal(vOut), vWi;
			vWi = Sampling::CosineSampleHemisphere(sample.u, sample.v);

			if (vWo.z < 0.0f)
				vWi.z *= -1.0f;

			*pvIn = diffGoem.LocalToWorld(vWi);
			*pPdf = Pdf(vWo, vWi, types);
			if (pSampledTypes != NULL)
			{
				*pSampledTypes = mScatterType;
			}

			return GetColor(diffGoem) * Eval(vWo, vWi, types);
		}

		// -----------------------------------------------------------------------------------------------------------------------
		// Mirror Implementation
		// -----------------------------------------------------------------------------------------------------------------------
		Color Mirror::Eval(const Vector3& vOut, const Vector3& vIn, const DifferentialGeom& diffGoem, ScatterType types) const
		{
			return Color::BLACK;
		}

		float Mirror::Eval(const Vector3& vOut, const Vector3& vIn, ScatterType types) const
		{
			return 0.0f;
		}

		float Mirror::Pdf(const Vector3& vIn, const Vector3& vOut, const DifferentialGeom& diffGoem, ScatterType types /* = BSDF_ALL */) const
		{
			return 0.0f;
		}

		float Mirror::Pdf(const Vector3& vIn, const Vector3& vOut, ScatterType types /* = BSDF_ALL */) const
		{
			return 0.0f;
		}

		Color Mirror::SampleScattered(const Vector3& vOut, const Sample& sample, const DifferentialGeom& diffGoem, Vector3* pvIn, float* pPdf, ScatterType types /* = BSDF_ALL */, ScatterType* pSampledTypes /* = NULL */) const
		{
			if (!MatchesTypes(types))
			{
				*pPdf = 0.0f;
				return Color::BLACK;
			}

			Vector3 vWo = diffGoem.WorldToLocal(vOut), vWi;
			vWi = Vector3(-vWo.x, -vWo.y, vWo.z);

			*pvIn = diffGoem.LocalToWorld(vWi);
			*pPdf = 1.0f;
			if (pSampledTypes != NULL)
			{
				*pSampledTypes = mScatterType;
			}

			return GetColor(diffGoem) / BSDFCoordinate::AbsCosTheta(vWi);
		}

		// -----------------------------------------------------------------------------------------------------------------------
		// Glass Implementation
		// -----------------------------------------------------------------------------------------------------------------------
		Color Glass::Eval(const Vector3& vOut, const Vector3& vIn, const DifferentialGeom& diffGoem, ScatterType types) const
		{
			return Color::BLACK;
		}

		float Glass::Eval(const Vector3& vOut, const Vector3& vIn, ScatterType types) const
		{
			return 0.0f;
		}

		float Glass::Pdf(const Vector3& vIn, const Vector3& vOut, const DifferentialGeom& diffGoem, ScatterType types /* = BSDF_ALL */) const
		{
			return 0.0f;
		}

		float Glass::Pdf(const Vector3& vIn, const Vector3& vOut, ScatterType types /* = BSDF_ALL */) const
		{
			return 0.0f;
		}

		Color Glass::SampleScattered(const Vector3& vOut, const Sample& sample, const DifferentialGeom& diffGoem, Vector3* pvIn, float* pPdf, ScatterType types /* = BSDF_ALL */, ScatterType* pSampledTypes /* = NULL */) const
		{
			bool sampleReflect = (types & (BSDF_REFLECTION | BSDF_SPECULAR)) == (BSDF_REFLECTION | BSDF_SPECULAR);
			bool sampleRefract = (types & (BSDF_TRANSMISSION | BSDF_SPECULAR)) == (BSDF_TRANSMISSION | BSDF_SPECULAR);

			if (!sampleReflect && !sampleRefract)
			{
				*pPdf = 0.0f;
				return Color::BLACK;
			}

			bool sampleBoth = sampleReflect == sampleRefract;

			Vector3 vWo = diffGoem.WorldToLocal(vOut), vWi;

			float fresnel = Fresnel(BSDFCoordinate::CosTheta(vWo), mEtai, mEtat);
			float prob = 0.5f * fresnel + 0.25f;

			if (sample.w <= prob && sampleBoth || (sampleReflect && !sampleBoth)) // Sample reflection
			{
				vWi = Vector3(-vWo.x, -vWo.y, vWo.z);

				*pvIn = diffGoem.LocalToWorld(vWi);
				*pPdf = !sampleBoth ? 1.0f : prob;
				if (pSampledTypes != NULL)
				{
					*pSampledTypes = ScatterType(BSDF_REFLECTION | BSDF_SPECULAR);
				}

				return fresnel * GetColor(diffGoem) / BSDFCoordinate::AbsCosTheta(vWi);
			}
			else if (sample.w > prob && sampleBoth || (sampleRefract && !sampleBoth)) // Sample refraction
			{
				bool entering = BSDFCoordinate::CosTheta(vWo) > 0.0f;
				float etai = mEtai, etat = mEtat;
				if (!entering)
					swap(etai, etat);

				float sini2 = BSDFCoordinate::SinTheta2(vWo);
				float eta = etai / etat;
				float sint2 = eta * eta * sini2;

				if (sint2 > 1.0f)
					return Color::BLACK;

				float cost = Math::Sqrt(Math::Max(0.0f, 1.0f - sint2));
				if (entering)
					cost = -cost;
				float sintOverSini = eta;

				vWi = Vector3(sintOverSini * -vWo.x, sintOverSini * -vWo.y, cost);

				*pvIn = diffGoem.LocalToWorld(vWi);
				*pPdf = !sampleBoth ? 1.0f : 1.0f - prob;
				if (pSampledTypes != NULL)
				{
					*pSampledTypes = ScatterType(BSDF_TRANSMISSION | BSDF_SPECULAR);
				}

				return (1.0f - fresnel) * GetColor(diffGoem) / BSDFCoordinate::AbsCosTheta(vWi);
			}

			return Color::BLACK;
		}
	}
}