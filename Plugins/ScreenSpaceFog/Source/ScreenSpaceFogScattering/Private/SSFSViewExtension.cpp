// Copyright 2025 Dmitry Karpukhin. All Rights Reserved.

#include "SSFSViewExtension.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "RenderGraph.h"
#include "SceneCore.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "Runtime/Engine/Classes/Engine/Texture.h"
#include "TextureResource.h"

// Console variables
TAutoConsoleVariable<int32> CVarSSFS(
TEXT("r.SSFS"),
true,
TEXT("Enable or disable the Screen Space Fog Scattering post process effect."),
ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<int32> CVarSSFSPassAmount(
TEXT("r.SSFS.PassAmount"),
8,
TEXT("Number of passes to render the Scattering effect.\n")
TEXT("Recommended values: 8 for Full HD, 9 for QHD, 10 for 4K resolutions.\n")
TEXT("Max number of passes is clamped to 12."),
ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<float> CVarSSFSRadius(
TEXT("r.SSFS.Radius"),
0.9,
TEXT("The progressive radius of the scattering effect.\n")
TEXT("Can be used to visibly increase (or decrease) the effect while staying with certain fog density.\n")
TEXT("Recommended values are between 0.8-0.99.\n"),
ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarSSFSIntensity(
TEXT("r.SSFS.Intensity"),
1.0,
TEXT("Intensity of the effect scaled by the Exponential Height Fog density values."),
ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarSSFSSkyAtmosphere(
TEXT("r.SSFS.SkyAtmosphere"),
0,
TEXT("Whether to support Sky Atmosphere by the shader."),
ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<float> CVarSSFSSkyAtmosphereIntensity(
TEXT("r.SSFS.SkyAtmosphereIntensity"),
1.0,
TEXT("Intensity of the effect scaled by the Sky Atmosphere."),
ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarSSFSVolumetricCloud(
TEXT("r.SSFS.VolumetricCloud"),
0,
TEXT("Whether to support Volumetric Clouds by the shader.\n")
TEXT("Experimental! Works only with 'r.VolumetricRenderTarget.Mode 0'."),
ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<float> CVarSSFSVolumetricCloudIntensity(
TEXT("r.SSFS.VolumetricCloudIntensity"),
1.0,
TEXT("Intensity of the effect scaled by the Volumetric Cloud density."),
ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarSSFSVolumetricFogILSIntensity(
TEXT("r.SSFS.VolumetricFogILSIntensity"),
1.0,
TEXT("Intensity of the Volumetric Fog Integrated Light Scattering on the effect."),
ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarSSFSHeterogeneousVolumes(
TEXT("r.SSFS.HeterogeneousVolumes"),
1,
TEXT("Whether to support Heterogeneous Volumes by the shader."),
ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<int32> CVarSSFSLuminanceWeighting(
TEXT("r.SSFS.LuminanceWeighting"),
1,
TEXT("Use additional filtering to reduce overly bright subpixels (fireflies)."),
ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarSSFSHighQuality(
TEXT("r.SSFS.HighQuality"),
1,
TEXT("Use High Quality sampling for the scattering effect.\n")
TEXT("Disabling can save on performance, but will reduce the quality."),
ECVF_RenderThreadSafe | ECVF_Scalability);

TAutoConsoleVariable<int32> CVarSSFSFullPrecision(
TEXT("r.SSFS.FullPrecision"),
1,
TEXT("Enables using full precision HDR render passes for the scattering effect.\n")
TEXT("Disabling can save on performance, but might introduce some color loss."),
ECVF_RenderThreadSafe | ECVF_Scalability);

// Shader declarations
namespace
{
	BEGIN_SHADER_PARAMETER_STRUCT(FCommonParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER(FIntRect, ViewportRect)
		SHADER_PARAMETER(FVector2f, ViewportInvSize)
		SHADER_PARAMETER(FVector2f, UVScale)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
	END_SHADER_PARAMETER_STRUCT()

	// We can't include FFogUniformParameters from FogRendering.h directly,
	// but we can make an impostor and feed him the same data...
	BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FFogUniformParametersSSFS,)
		SHADER_PARAMETER(FVector4f, ExponentialFogParameters)
		SHADER_PARAMETER(FVector4f, ExponentialFogParameters2)
		SHADER_PARAMETER(FVector4f, ExponentialFogColorParameter)
		SHADER_PARAMETER(FVector4f, ExponentialFogParameters3)
		SHADER_PARAMETER(FVector4f, SkyAtmosphereAmbientContributionColorScale)
		SHADER_PARAMETER(FVector4f, InscatteringLightDirection) // non negative DirectionalInscatteringStartDistance in .W
		SHADER_PARAMETER(FVector4f, DirectionalInscatteringColor)
		SHADER_PARAMETER(FVector2f, SinCosInscatteringColorCubemapRotation)
		SHADER_PARAMETER(FVector3f, FogInscatteringTextureParameters)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		SHADER_PARAMETER(float, EndDistance)
#endif
		SHADER_PARAMETER(float, ApplyVolumetricFog)
		SHADER_PARAMETER(float, VolumetricFogStartDistance)
		SHADER_PARAMETER(float, VolumetricFogNearFadeInDistanceInv)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		SHADER_PARAMETER(FVector3f, VolumetricFogAlbedo)
		SHADER_PARAMETER(float, VolumetricFogPhaseG)
#endif
		SHADER_PARAMETER_TEXTURE(TextureCube, FogInscatteringColorCubemap)
		SHADER_PARAMETER_SAMPLER(SamplerState, FogInscatteringColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, IntegratedLightScattering)
		SHADER_PARAMETER_SAMPLER(SamplerState, IntegratedLightScatteringSampler)
	END_GLOBAL_SHADER_PARAMETER_STRUCT()

	IMPLEMENT_UNIFORM_BUFFER_STRUCT(FFogUniformParametersSSFS, "FogStructSSFS");

	class FSetupCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSetupCS);
		SHADER_USE_PARAMETER_STRUCT(FSetupCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FCommonParameters, CommonParameters)
			SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FFogUniformParametersSSFS, FogStruct)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTex)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HeterogeneousVolumeTex)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, VolumetricCloudTex)
			SHADER_PARAMETER(float, Intensity)
			SHADER_PARAMETER(float, SkyAtmosphereIntensity)
			SHADER_PARAMETER(float, VolumetricCloudIntensity)
			SHADER_PARAMETER(float, VolumetricFogILSIntensity)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

		class FSupportHeightFog							: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_HEIGHT_FOG");
		class FSupportFogStartDistance					: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_FOG_START_DISTANCE");
		class FSupportFogInScatteringTexture			: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_FOG_INSCATTERING_TEXTURE");
		class FSupportFogSecondTerm						: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_FOG_SECOND_TERM");
		class FSupportFogDirectionalLightInScattering	: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_FOG_DIRECTIONAL_LIGHT_INSCATTERING");
		class FSupportVolumetricFog						: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_VOLUMETRIC_FOG");
		class FSupportAerialPerspective					: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_AERIAL_PERSPECTIVE");
		class FSupportHeterogeneousVolume				: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_HETEROGENEOUS_VOLUME");
		class FSupportVolumetricCloud					: SHADER_PERMUTATION_BOOL("PERMUTATION_SUPPORT_VOLUMETRIC_CLOUD");
		
		using FPermutationDomain = TShaderPermutationDomain< 
			FSupportHeightFog, 
			FSupportFogStartDistance,
			FSupportFogInScatteringTexture, 
			FSupportFogSecondTerm,
			FSupportFogDirectionalLightInScattering,
			FSupportVolumetricFog,
			FSupportAerialPerspective,
			FSupportHeterogeneousVolume,
			FSupportVolumetricCloud
		>;

		static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
		{
			if (PermutationVector.Get<FSupportHeightFog>() == false)
			{
				PermutationVector.Set<FSupportFogInScatteringTexture>(false);
				PermutationVector.Set<FSupportFogDirectionalLightInScattering>(false);
				PermutationVector.Set<FSupportFogStartDistance>(false);
				PermutationVector.Set<FSupportFogSecondTerm>(false);
			}
			return PermutationVector;
		}

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

			// Pass engine version to the shader
			OutEnvironment.SetDefine(TEXT("ENGINE_MAJOR_VERSION"), ENGINE_MAJOR_VERSION);
			OutEnvironment.SetDefine(TEXT("ENGINE_MINOR_VERSION"), ENGINE_MINOR_VERSION);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FSetupCS, "/Plugins/ScreenSpaceFogScattering/SSFSSetup.usf", "SetupCS", SF_Compute);

	class FDownsampleCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDownsampleCS);
		SHADER_USE_PARAMETER_STRUCT(FDownsampleCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FCommonParameters, CommonParameters)
			SHADER_PARAMETER(int, PassNumber)
			SHADER_PARAMETER(FVector2f, InputSize)
			SHADER_PARAMETER(float, HighQuality)
			SHADER_PARAMETER(float, LuminanceWeighting)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FDownsampleCS, "/Plugins/ScreenSpaceFogScattering/SSFSDownsample.usf", "DownsampleCS", SF_Compute);

	class FUpsampleCombineCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FUpsampleCombineCS);
		SHADER_USE_PARAMETER_STRUCT(FUpsampleCombineCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FCommonParameters, CommonParameters)
			SHADER_PARAMETER(FVector2f, InputSize)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, PreviousTexture)
			SHADER_PARAMETER(float, Radius)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FUpsampleCombineCS, "/Plugins/ScreenSpaceFogScattering/SSFSUpsampleCombine.usf", "UpsampleCombineCS", SF_Compute);

	class FRecombineCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FRecombineCS);
		SHADER_USE_PARAMETER_STRUCT(FRecombineCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_STRUCT_INCLUDE(FCommonParameters, CommonParameters)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ScatteringTexture)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SetupTexture)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Output)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};
	IMPLEMENT_GLOBAL_SHADER(FRecombineCS, "/Plugins/ScreenSpaceFogScattering/SSFSRecombine.usf", "RecombineCS", SF_Compute);
}

DECLARE_GPU_STAT_NAMED(SSFS, TEXT("Screen Space Fog Scattering"))

FScreenSpaceFogScatteringViewExtension::FScreenSpaceFogScatteringViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister)
{
	UE_LOG(LogTemp, Log, TEXT("SceneViewExtension: Screen Space Fog Scattering is registered"));
}

// Render the SSFS at the start of the post processing pipeline (before DOF) ensuring that it's compatible with any other default post process effects
void FScreenSpaceFogScatteringViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
{
	FSceneViewExtensionBase::PrePostProcessPass_RenderThread(GraphBuilder, View, Inputs);
	
	const FIntRect Viewport = static_cast<const FViewInfo&>(View).ViewRect;
	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
	const FScreenPassTexture SceneColor((*Inputs.SceneTextures)->SceneColorTexture, Viewport);
	FScreenPassTexture SSFSOutput;

	const int32 MaxPassAmount = 12;
	PassAmount = FMath::Clamp(CVarSSFSPassAmount.GetValueOnRenderThread(), 0, MaxPassAmount);
	
	const bool Validity = CVarSSFS.GetValueOnRenderThread() == 1 && PassAmount > 1 &&
			CVarSSFSIntensity.GetValueOnRenderThread() > 0 && CVarSSFSRadius.GetValueOnRenderThread() > 0 &&
			View.Family->Scene->HasAnyExponentialHeightFog() && View.Family->ViewMode != VMI_PathTracing;

	if (SceneColor.IsValid() && Validity)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ScreenSpaceFogScattering %dx%d (PassAmount=%d)", Viewport.Width(), Viewport.Height(), PassAmount);
		RenderSSFS(GraphBuilder, ViewInfo, Inputs, SSFSOutput);
		AddCopyTexturePass(GraphBuilder, SSFSOutput.Texture, SceneColor.Texture);
	}
}

void FScreenSpaceFogScatteringViewExtension::RenderSSFS(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo, const FPostProcessingInputs& Inputs, FScreenPassTexture& Output)
{
	const FIntRect Viewport = ViewInfo.ViewRect;
	FScreenPassTexture SceneColor((*Inputs.SceneTextures)->SceneColorTexture, Viewport);
	FScreenPassTexture SceneDepth((*Inputs.SceneTextures)->SceneDepthTexture, Viewport);
	
	RDG_GPU_STAT_SCOPE(GraphBuilder, SSFS)

	FScreenPassTexture ScatteringTexture;
	
	// Setup
	FRDGTextureRef SetupTexture = Setup(GraphBuilder, ViewInfo, SceneColor.Texture, SceneDepth.Texture, SceneColor.ViewRect);

	// Both Downsample and Upsample methods for rendering Bloom are described here: http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
	// You can learn about the Unreal implementation of this in detail here: https://www.froyok.fr/blog/2021-12-ue4-custom-bloom/
	// It's also a perfect fit for the Fog Scattering shader
	{
		RDG_EVENT_SCOPE(GraphBuilder, "SSFS Downsample");
		
		// Downsample
		int32 Width = ViewInfo.ViewRect.Width();
		int32 Height = ViewInfo.ViewRect.Height();
		int32 Divider = 1;
		FRDGTextureRef PreviousTexture = SetupTexture;

		for(int32 i = 0; i < PassAmount; i++)
		{
			FIntRect Size{0,0,FMath::Max(Width / Divider, 2),FMath::Max(Height / Divider, 2)};

			const FString PassName = "Downsample (1/" 
			+ FString::FromInt(Divider)
			+ ") "
			+ FString::FromInt(Size.Width()) + "x"
			+ FString::FromInt(Size.Height());

			const FString* TextureName = GraphBuilder.AllocObject<FString>("SSFS.Downsample(1/" 
			+ FString::FromInt(Divider)
			+ ")");

			FRDGTextureRef Texture = nullptr;
		
			if (i == 0)
			{
				Texture = PreviousTexture;
			}
			else
			{
				Texture = Downsample(GraphBuilder, ViewInfo, i, PassName, TextureName, PreviousTexture, Size);
			}

			FScreenPassTexture DownsampleTexture(Texture, Size);

			DownsampleMipMaps.Add(DownsampleTexture);
			PreviousTexture = Texture;
			Divider *= 2;
		}
	}

	{
		RDG_EVENT_SCOPE(GraphBuilder, "SSFS Upsample");
	
		// Upsample
		float Radius = FMath::Clamp(CVarSSFSRadius.GetValueOnRenderThread(), 0, 1);
		
		UpsampleMipMaps.Append(DownsampleMipMaps);
		
		for(int32 i = PassAmount - 2; i >= 0; i--)
		{
			FIntRect CurrentSize = UpsampleMipMaps[i].ViewRect;
			//FIntRect CurrentSize = UpsampleMipMaps[i].Texture->Desc.Extent.XY;

			const FString PassName  = "Upsample & Combine ("
			+ FString::FromInt(PassAmount - 1 - i)
			+ "/"
			+ FString::FromInt(PassAmount - 1)
			+ ") "
			+ FString::FromInt(CurrentSize.Width())
			+ "x"
			+ FString::FromInt(CurrentSize.Height());

			const FString* TextureName = GraphBuilder.AllocObject<FString>("SSFS.Upsample(" 
			+ FString::FromInt(PassAmount - 1 - i)
			+ "/"
			+ FString::FromInt(PassAmount - 1)
			+ ")"
			);

			FRDGTextureRef ResultTexture = UpsampleCombine(GraphBuilder, ViewInfo, PassName, TextureName, UpsampleMipMaps[i], UpsampleMipMaps[i + 1], Radius);

			FScreenPassTexture NewTexture(ResultTexture, CurrentSize);
			UpsampleMipMaps[i] = NewTexture;
		}

		ScatteringTexture = UpsampleMipMaps[0];
	}

	// Recombine
	FRDGTextureRef CombineTexture = Recombine(GraphBuilder, ViewInfo, SceneColor, ScatteringTexture.Texture, SetupTexture, SceneColor.ViewRect);
	
	// Reset texture lists
	DownsampleMipMaps.Empty();
	UpsampleMipMaps.Empty();

	// Output
	Output.Texture = CombineTexture;
	Output.ViewRect = SceneColor.ViewRect;
}

void SetupFogUniformParametersSSFS(FRDGBuilder& GraphBuilder, const FViewInfo& View, FFogUniformParametersSSFS& OutParameters)
{
	// Exponential Height Fog
	{
		const FTexture* Cubemap = GWhiteTextureCube;

		if (View.FogInscatteringColorCubemap)
		{
			Cubemap = View.FogInscatteringColorCubemap->GetResource();
		}

		OutParameters.ExponentialFogParameters = View.ExponentialFogParameters;
		OutParameters.ExponentialFogColorParameter = FVector4f(View.ExponentialFogColor, 1.0f - View.FogMaxOpacity);
		OutParameters.ExponentialFogParameters2 = View.ExponentialFogParameters2;
		OutParameters.ExponentialFogParameters3 = View.ExponentialFogParameters3;
		OutParameters.SkyAtmosphereAmbientContributionColorScale = View.SkyAtmosphereAmbientContributionColorScale;
		OutParameters.SinCosInscatteringColorCubemapRotation = View.SinCosInscatteringColorCubemapRotation;
		OutParameters.FogInscatteringTextureParameters = (FVector3f)View.FogInscatteringTextureParameters;
		OutParameters.InscatteringLightDirection = (FVector3f)View.InscatteringLightDirection;
		OutParameters.InscatteringLightDirection.W = View.bUseDirectionalInscattering ? FMath::Max(0.f, View.DirectionalInscatteringStartDistance) : -1.f;
		OutParameters.DirectionalInscatteringColor = FVector4f(FVector3f(View.DirectionalInscatteringColor), FMath::Clamp(View.DirectionalInscatteringExponent, 0.000001f, 1000.0f));
		OutParameters.FogInscatteringColorCubemap = Cubemap->TextureRHI;
		OutParameters.FogInscatteringColorSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		OutParameters.EndDistance = View.FogEndDistance;
#endif
	}

	// Volumetric Fog
	{
		if (View.VolumetricFogResources.IntegratedLightScatteringTexture)
		{
			OutParameters.IntegratedLightScattering = View.VolumetricFogResources.IntegratedLightScatteringTexture;
			OutParameters.ApplyVolumetricFog = 1.0f;
		}
		else
		{
			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			OutParameters.IntegratedLightScattering = SystemTextures.VolumetricBlackAlphaOne;
			OutParameters.ApplyVolumetricFog = 0.0f;
		}
		OutParameters.IntegratedLightScatteringSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutParameters.VolumetricFogStartDistance = View.VolumetricFogStartDistance;
		OutParameters.VolumetricFogNearFadeInDistanceInv = View.VolumetricFogNearFadeInDistanceInv;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
		OutParameters.VolumetricFogPhaseG = View.VolumetricFogPhaseG;
		OutParameters.VolumetricFogAlbedo = View.VolumetricFogAlbedo;
#endif
	}
}

TRDGUniformBufferRef<FFogUniformParametersSSFS> CreateFogUniformBufferSSFS(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	auto* FogStruct = GraphBuilder.AllocParameters<FFogUniformParametersSSFS>();
	SetupFogUniformParametersSSFS(GraphBuilder, View, *FogStruct);
	return GraphBuilder.CreateUniformBuffer(FogStruct);
}

static bool IsVolumetricCloudRenderTargetValid()
{
	static const auto bVolumetricCloudRenderTarget = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VolumetricRenderTarget"));
	static const auto bVolumetricCloudRenderTargetMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VolumetricRenderTarget.Mode"));
	
	if (!bVolumetricCloudRenderTarget || !bVolumetricCloudRenderTargetMode)
	{
		return false;
	}
	
	return bVolumetricCloudRenderTarget->GetValueOnAnyThread() == 1 && bVolumetricCloudRenderTargetMode->GetValueOnAnyThread() == 0;
}

// Setup pass: SceneColor * HeightFog in RGB, HeightFog in A
FRDGTextureRef FScreenSpaceFogScatteringViewExtension::Setup(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef InputTexture, FRDGTextureRef DepthTexture, const FIntRect& ViewRect)
{
	FRDGTextureDesc Description = InputTexture->Desc;
	Description.Reset();
	Description.Flags |= TexCreate_UAV;
	Description.Flags &= ~(TexCreate_RenderTargetable | TexCreate_FastVRAM);
	Description.Extent = ViewRect.Size();
	Description.Format = PF_FloatRGBA;
	Description.ClearValue = FClearValueBinding(FLinearColor::Black);
	const FRDGTextureRef TargetTexture = GraphBuilder.CreateTexture(Description, TEXT("SSFS.Setup"));

	FScene* Scene = View.Family->Scene->GetRenderScene();
	const FEngineShowFlags EngineShowFlags = View.Family->EngineShowFlags;
	
	const float Intensity = CVarSSFSIntensity.GetValueOnRenderThread();
	const float SkyAtmosphereIntensity = CVarSSFSSkyAtmosphereIntensity.GetValueOnRenderThread();
	const float VolumetricFogILSIntensity = CVarSSFSVolumetricFogILSIntensity.GetValueOnRenderThread();
	const float VolumetricCloudIntensity = CVarSSFSVolumetricCloudIntensity.GetValueOnRenderThread();
	const bool bUseSkyAtmosphere = CVarSSFSSkyAtmosphere.GetValueOnRenderThread() > 0;
	const bool bUseHeterogeneousVolumes = CVarSSFSHeterogeneousVolumes.GetValueOnRenderThread() > 0;
	const bool bUseVolumetricCloud = CVarSSFSVolumetricCloud.GetValueOnRenderThread() > 0;
	
	bool bVolumetricCloudRenderTargetValid = IsVolumetricCloudRenderTargetValid();
	
	const bool bSupportHeightFog = Scene->ExponentialFogs.Num() > 0 && View.Family->EngineShowFlags.Fog;
	const bool bSupportFogStartDistance = View.ExponentialFogParameters.W > 0;
	const bool bSupportFogInscatteringColorCubemap = View.FogInscatteringColorCubemap != nullptr;
	const bool bSupportFogSecondTerm = View.ExponentialFogParameters2.X > 0;
	const bool bSupportFogDirectionalInscatering = !bSupportFogInscatteringColorCubemap && (View.DirectionalInscatteringColor.GetLuminance() > 0 || View.bUseDirectionalInscattering);
	const bool bSupportVolumetricFog = Scene->ExponentialFogs[0].bEnableVolumetricFog && View.Family->EngineShowFlags.VolumetricFog;
	const bool bSupportAerialPerspective = bUseSkyAtmosphere && Scene->HasSkyAtmosphere() && SkyAtmosphereIntensity > 0 && EngineShowFlags.Atmosphere;
	const bool bSupportHeterogeneousVolumes = bUseHeterogeneousVolumes && View.HeterogeneousVolumeRadiance && EngineShowFlags.HeterogeneousVolumes;
	const bool bSupportVolumetricCloud = bUseVolumetricCloud && bVolumetricCloudRenderTargetValid && Scene->HasVolumetricCloud() && VolumetricCloudIntensity > 0 && EngineShowFlags.Cloud;
	
	FSetupCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FSetupCS::FSupportHeightFog>(bSupportHeightFog);
	PermutationVector.Set<FSetupCS::FSupportFogStartDistance>(bSupportFogStartDistance);
	PermutationVector.Set<FSetupCS::FSupportFogInScatteringTexture>(bSupportFogInscatteringColorCubemap);
	PermutationVector.Set<FSetupCS::FSupportFogSecondTerm>(bSupportFogSecondTerm);
	PermutationVector.Set<FSetupCS::FSupportFogDirectionalLightInScattering>(bSupportFogDirectionalInscatering);
	PermutationVector.Set<FSetupCS::FSupportVolumetricFog>(bSupportVolumetricFog);
	PermutationVector.Set<FSetupCS::FSupportAerialPerspective>(bSupportAerialPerspective);
	PermutationVector.Set<FSetupCS::FSupportHeterogeneousVolume>(bSupportHeterogeneousVolumes);
	PermutationVector.Set<FSetupCS::FSupportVolumetricCloud>(bSupportVolumetricCloud);

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	const TShaderMapRef<FSetupCS> ComputeShader(GlobalShaderMap, PermutationVector);
	FSetupCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetupCS::FParameters>();
	
	const FIntRect PassViewSize = View.ViewRect;
	const FIntPoint SrcTextureSize = InputTexture->Desc.Extent;
	
	TRDGUniformBufferRef<FFogUniformParametersSSFS> FogUniformBuffer = CreateFogUniformBufferSSFS(GraphBuilder, View);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	PassParameters->CommonParameters.InputTexture = InputTexture;
	PassParameters->CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI();
	PassParameters->FogStruct = FogUniformBuffer;
	PassParameters->DepthTex = DepthTexture;
	PassParameters->HeterogeneousVolumeTex = View.HeterogeneousVolumeRadiance ? View.HeterogeneousVolumeRadiance : SystemTextures.Black;
	PassParameters->VolumetricCloudTex = bVolumetricCloudRenderTargetValid && bSupportVolumetricCloud ? View.State->GetVolumetricCloudTexture(GraphBuilder) : SystemTextures.Black;
	PassParameters->Intensity = Intensity;
	PassParameters->SkyAtmosphereIntensity = SkyAtmosphereIntensity;
	PassParameters->VolumetricFogILSIntensity = VolumetricFogILSIntensity;
	PassParameters->VolumetricCloudIntensity = VolumetricCloudIntensity;
	
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TargetTexture));
	
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(PassViewSize.Size(), DefaultGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SSFS Setup %dx%d", PassViewSize.Width(), PassViewSize.Height()),
		ComputeShader,
		PassParameters,
		GroupCount);

	return TargetTexture;
}

// Scattering 13-tap downsample pass
FRDGTextureRef FScreenSpaceFogScatteringViewExtension::Downsample(FRDGBuilder& GraphBuilder, const FViewInfo& View, const int32 PassNumber, const FString& PassName, const FString* TextureName, FRDGTextureRef InputTexture, const FIntRect& ViewRect)
{
	FRDGTextureDesc Description = InputTexture->Desc;
	Description.Reset();
	Description.Flags |= TexCreate_UAV;
	Description.Flags &= ~(TexCreate_RenderTargetable | TexCreate_FastVRAM);
	Description.Extent = ViewRect.Size();
	Description.Format = CVarSSFSFullPrecision.GetValueOnRenderThread() ? PF_FloatRGBA : PF_FloatR11G11B10;
	Description.ClearValue = FClearValueBinding(FLinearColor::Black);
	const FRDGTextureRef TargetTexture = GraphBuilder.CreateTexture(Description, **TextureName);

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	const TShaderMapRef<FDownsampleCS> ComputeShader(GlobalShaderMap);
	FDownsampleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDownsampleCS::FParameters>();
	
	const FIntRect PassViewSize = View.ViewRect;
	const FIntPoint SrcTextureSize = Description.Extent;

	const float HighQuality = CVarSSFSHighQuality.GetValueOnRenderThread();
	const float LuminanceWeighting = CVarSSFSLuminanceWeighting.GetValueOnRenderThread();
	
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	PassParameters->CommonParameters.InputTexture = InputTexture;
	PassParameters->CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PassParameters->PassNumber = PassNumber;
	PassParameters->InputSize = FVector2f(ViewRect.Width(), ViewRect.Height()); // ViewRect.Size()
	PassParameters->HighQuality = HighQuality;
	PassParameters->LuminanceWeighting = LuminanceWeighting;
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TargetTexture));
	
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(SrcTextureSize, DefaultGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("%s", *PassName),
		ComputeShader,
		PassParameters,
		GroupCount);

	return TargetTexture;
}

// Scattering 9-tap upsample & combine pass
FRDGTextureRef FScreenSpaceFogScatteringViewExtension::UpsampleCombine(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FString& PassName, const FString* TextureName, const FScreenPassTexture& InputTexture, const FScreenPassTexture& PreviousTexture, float Radius)
{
	FRDGTextureDesc Description = InputTexture.Texture->Desc;
	Description.Reset();
	Description.Flags |= TexCreate_UAV;
	Description.Flags &= ~(TexCreate_RenderTargetable | TexCreate_FastVRAM);
	Description.Extent = InputTexture.ViewRect.Size();
	Description.Format = CVarSSFSFullPrecision.GetValueOnRenderThread() ? PF_FloatRGBA : PF_FloatR11G11B10;
	Description.ClearValue = FClearValueBinding(FLinearColor::Black);
	const FRDGTextureRef TargetTexture = GraphBuilder.CreateTexture(Description, **TextureName);

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	const TShaderMapRef<FUpsampleCombineCS> ComputeShader(GlobalShaderMap);
	FUpsampleCombineCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpsampleCombineCS::FParameters>();
	
	const FIntRect PassViewSize = View.ViewRect;
	const FIntPoint SrcTextureSize = Description.Extent;
	
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	PassParameters->CommonParameters.InputTexture = InputTexture.Texture;
	PassParameters->CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	
	PassParameters->PreviousTexture = PreviousTexture.Texture;
	PassParameters->InputSize = FVector2f(PreviousTexture.ViewRect.Width(), PreviousTexture.ViewRect.Height()); // PreviousTexture.ViewRect.Size()
	PassParameters->Radius = Radius;
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TargetTexture));
	
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(SrcTextureSize, DefaultGroupSize);
	
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("%s", *PassName),
		ComputeShader,
		PassParameters,
		GroupCount);
	
	return TargetTexture;
}

// Final recombine pass: lerp(SceneColor, ScatteringColor, HeightFog)
FRDGTextureRef FScreenSpaceFogScatteringViewExtension::Recombine(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FScreenPassTexture& SceneColor, FRDGTextureRef ScatteringTexture, FRDGTextureRef SetupTexture, const FIntRect& ViewRect)
{
	FRDGTextureDesc Description = SceneColor.Texture->Desc;
	Description.Reset();
	Description.Flags |= TexCreate_UAV;
	Description.Flags &= ~(TexCreate_RenderTargetable | TexCreate_FastVRAM);
	Description.Extent = SceneColor.Texture->Desc.Extent;
	//Description.Format = PF_FloatRGBA;
	Description.ClearValue = FClearValueBinding(FLinearColor::Black);
	const FRDGTextureRef TargetTexture = GraphBuilder.CreateTexture(Description, TEXT("SSFS.Recombine"));

	const FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(View.FeatureLevel);
	const TShaderMapRef<FRecombineCS> ComputeShader(GlobalShaderMap);
	FRecombineCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRecombineCS::FParameters>();
	
	const FIntRect PassViewSize = ViewRect;
	const FIntPoint SrcTextureSize = Description.Extent;
	
	PassParameters->CommonParameters.ViewUniformBuffer = View.ViewUniformBuffer;
	PassParameters->CommonParameters.ViewportRect = PassViewSize;
	PassParameters->CommonParameters.ViewportInvSize = FVector2f(1.0f / PassViewSize.Width(), 1.0f / PassViewSize.Height());
	PassParameters->CommonParameters.UVScale = FVector2f(float(PassViewSize.Width()) / float(SrcTextureSize.X), float(PassViewSize.Height()) / float(SrcTextureSize.Y));
	PassParameters->CommonParameters.InputTexture = SceneColor.Texture;
	PassParameters->CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	
	PassParameters->ScatteringTexture = ScatteringTexture;
	PassParameters->SetupTexture = SetupTexture;
	PassParameters->Output = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TargetTexture));
	
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(PassViewSize.Size(), DefaultGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SSFS Recombine %dx%d", PassViewSize.Width(), PassViewSize.Height()),
		ComputeShader,
		PassParameters,
		GroupCount);
	
	return TargetTexture;
}
