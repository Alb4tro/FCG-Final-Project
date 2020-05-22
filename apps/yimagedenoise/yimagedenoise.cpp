#include <yocto/yocto_commonio.h>
#include <yocto/yocto_image.h>
#include <yocto_gui/yocto_gui.h>

#include "Denoiser.h"
#include "MultiscaleDenoiser.h"
#include "IDenoiser.h"
#include "SpikeRemovalFilter.h"
#include "ParametersIO.h"
#include "ImageIO.h"
#include "DeepImage.h"
#include "Chronometer.h"
#include "Utils.h"
#include "SamplesAccumulator.h"
#include "CovarianceMatrix.h"

#include <iostream>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <stdio.h>

using namespace yocto::math;
namespace img = yocto::image;
namespace cli = yocto::commonio;
namespace gui = yocto::gui;

using namespace std::string_literals;

#include <atomic>
#include <deque>
#include <future>

#include "ext/filesystem.hpp"
namespace sfs = ghc::filesystem;


namespace bcd 
{
  /*
  * Raw converter to bcd
  */

  static const char* g_pProgramName = "bcddenoiser";
  static const char* g_pColorSuffix = "";
  static const char* g_pHistogramSuffix = "_hist";
  static const char* g_pCovarianceSuffix = "_cov";
  static const char* g_pDeepImageExtension = ".exr";

  const float g_satureLevelGamma = 2.f; // used in histogram accumulation

  class ProgramArguments_raw2bcd
  {
	public:
		ProgramArguments_raw2bcd() :
				m_histogramNbOfBins(20),
				m_histogramGamma(2.2f),
				m_histogramMaxValue(2.5f)
		{}

	public:
		std::string m_inputFilePath; ///< File path to the input image
		std::string m_outputColorFilePath; ///< File path to the output color image
		std::string m_outputHistogramFilePath; ///< File path to the output histogram image
		std::string m_outputCovarianceFilePath; ///< File path to the output covariance image
		float m_histogramNbOfBins;
		float m_histogramGamma;
		float m_histogramMaxValue;
  };

  typedef struct
	{
		int32_t version;
		int32_t width;
		int32_t height;
		int32_t nbOfSamples;
		int32_t nbOfChannels;
	} RawFileHeader;

  bool parseProgramArguments_rawToBcd(std::string inputFilePath, std::string outputFilePathPrefix,
   ProgramArguments_raw2bcd& o_rProgramArguments, std::string& error)
  {
    o_rProgramArguments.m_inputFilePath = inputFilePath;
    if(!std::ifstream(o_rProgramArguments.m_inputFilePath)){
      error = "ERROR in program arguments: cannot open input file" + o_rProgramArguments.m_inputFilePath;
      return false;
    }

	outputFilePathPrefix = outputFilePathPrefix;
	o_rProgramArguments.m_outputColorFilePath = outputFilePathPrefix + g_pColorSuffix + g_pDeepImageExtension;
	o_rProgramArguments.m_outputHistogramFilePath = outputFilePathPrefix + g_pHistogramSuffix + g_pDeepImageExtension;
	o_rProgramArguments.m_outputCovarianceFilePath = outputFilePathPrefix + g_pCovarianceSuffix + g_pDeepImageExtension;

    if(!std::ofstream(o_rProgramArguments.m_outputColorFilePath)){
      error = "ERROR in program arguments: cannot write output file for color " + o_rProgramArguments.m_outputColorFilePath;
      return false;
    }
    if(!std::ofstream(o_rProgramArguments.m_outputHistogramFilePath)){
      error = "ERROR in program arguments: cannot write output file for histogram " + o_rProgramArguments.m_outputHistogramFilePath;
      return false;
    }
    if(!std::ofstream(o_rProgramArguments.m_outputCovarianceFilePath)){
      error = "ERROR in program arguments: cannot write output file for covariance " + o_rProgramArguments.m_outputCovarianceFilePath;
      return false;
    }
       
    return true;
  }

  void printHeader_raw2bcd(const RawFileHeader& i_rHeader)
	{
		std::cout << "Version: " << i_rHeader.version << std::endl;
		std::cout << "Resolution: " << i_rHeader.width << "x" << i_rHeader.height << std::endl;
		std::cout << "Nb of samples: " << i_rHeader.nbOfSamples << std::endl;
		std::cout << "Nb of channels: " << i_rHeader.nbOfChannels << std::endl;
	}

  int convertRawToBcd(std::string inputFilePath, std::string outputFilePathPrefix, std::string& error)
	{
		ProgramArguments_raw2bcd programArgs;
		if(!parseProgramArguments_rawToBcd(inputFilePath, outputFilePathPrefix, programArgs, error))
			return 1;

		std::ifstream inputFile(programArgs.m_inputFilePath.c_str(), std::ios::binary);
		RawFileHeader header;
		inputFile.read(reinterpret_cast<char*>(&header), sizeof(RawFileHeader));
		printHeader_raw2bcd(header);

		HistogramParameters histoParams;
		histoParams.m_nbOfBins = programArgs.m_histogramNbOfBins;
		histoParams.m_gamma = programArgs.m_histogramGamma;
		histoParams.m_maxValue = programArgs.m_histogramMaxValue;
		SamplesAccumulator samplesAccumulator(header.width, header.height, histoParams);

		float sample[4]; // assumes nbOfChannels can be only 3 or 4
		std::streamsize sampleSize = header.nbOfChannels * sizeof(float);
		for(int32_t line = 0; line < header.height; ++line)
		{
			for(int32_t col = 0; col < header.width; ++col)
			{
				for(int32_t sampleIndex = 0; sampleIndex < header.nbOfSamples; ++sampleIndex)
				{
					inputFile.read(reinterpret_cast<char*>(sample), sampleSize);
					samplesAccumulator.addSample(line, col, sample[0], sample[1], sample[2]);
				}
			}
		}

		SamplesStatisticsImages samplesStats = samplesAccumulator.extractSamplesStatistics();
		Deepimf histoAndNbOfSamplesImage = Utils::mergeHistogramAndNbOfSamples(samplesStats.m_histoImage, samplesStats.m_nbOfSamplesImage);

		samplesStats.m_histoImage.clearAndFreeMemory();
		samplesStats.m_nbOfSamplesImage.clearAndFreeMemory();

		ImageIO::writeEXR(samplesStats.m_meanImage, programArgs.m_outputColorFilePath.c_str());
		ImageIO::writeMultiChannelsEXR(samplesStats.m_covarImage, programArgs.m_outputCovarianceFilePath.c_str());
		ImageIO::writeMultiChannelsEXR(histoAndNbOfSamplesImage, programArgs.m_outputHistogramFilePath.c_str());

		return 0;
	} // end raw converter to bcd

  /*
  * Bayesian Collaborative Denoise
  */
  static const char* g_pProgramPath;

  class ProgramArguments_bcd
  {
	public:
		ProgramArguments_bcd() :
			m_denoisedOutputFilePath(""),
			m_colorImage(), m_nbOfSamplesImage(), m_histogramImage(), m_covarianceImage(),
			m_histogramPatchDistanceThreshold(1.f),
			m_patchRadius(1), m_searchWindowRadius(6),
			m_minEigenValue(1.e-8f),
			m_useRandomPixelOrder(true),
			m_prefilterSpikes(true),
			m_prefilterThresholdStDevFactor(2.f),
			m_markedPixelsSkippingProbability(1.f),
			m_nbOfScales(3),
			m_nbOfCores(0)
		{}

	public:
		std::string m_denoisedOutputFilePath; ///< File path to the denoised image output
		Deepimf m_colorImage; ///< Pixel color values
		Deepimf m_nbOfSamplesImage; ///< Pixel number of samples
		Deepimf m_histogramImage; ///< Pixel histograms
		Deepimf m_covarianceImage; ///< Pixel covariances
		float m_histogramPatchDistanceThreshold; ///< Histogram patch distance threshold
		int m_patchRadius; ///< Patch has (1 + 2 x m_patchRadius)^2 pixels
		int m_searchWindowRadius; ///< Search windows (for neighbors) spreads across (1 + 2 x m_patchRadius)^2 pixels
		float m_minEigenValue; ///< Minimum eigen value for matrix inversion
		bool m_useRandomPixelOrder; ///< True means the pixel will be processed in a random order ; could be useful to remove some "grid" artifacts
		bool m_prefilterSpikes; ///< True means a spike removal prefiltering will be applied
		float m_prefilterThresholdStDevFactor; ///< See SpikeRemovalFilter::filter argument
		float m_markedPixelsSkippingProbability; ///< 1 means the marked centers of the denoised patches will be skipped to accelerate a lot the computations
		int m_nbOfScales;
		int m_nbOfCores; ///< Number of cores used by OpenMP. O means using the value defined in environment variable OMP_NUM_THREADS
  };

  	void initializeRandomSeed()
	{
		srand(static_cast<unsigned int>(time(0)));
	}

	bool parseBcdArguments(ProgramArguments_bcd& programArgs, std::string& error, std::string output, float hist_distance, int windows_radius, int patches_radius, 
		int random_order, int spike_removal, float factor, float skip_marked_patches, int multi_scaling, int core_to_use, float min_eigen_val)
	{
		std::string outputPath = "out/denoised_image.exr";
  		std::string inputColorPath = output + ".exr";
  		std::string inputHistPath = output + "_hist" + ".exr";
  		std::string inputCovariancePath = output + "_cov" + ".exr";

		// output path
    	programArgs.m_denoisedOutputFilePath = outputPath;
    	std::ofstream outputFile(programArgs.m_denoisedOutputFilePath, std::ofstream::out | std::ofstream::app);
		if(!outputFile){
			error =  "ERROR in program arguments: cannot write output file";
			return false;
		}

    	// input color path
    	const char* inputColorFilePath = inputColorPath.c_str();
    	if (!ImageIO::loadEXR(programArgs.m_colorImage, inputColorFilePath)){
			error = "ERROR in program arguments: couldn't load input color image file";
			return false;
		}
		std::cout << "Color image loaded" << "\n";

    	// input histogram path
    	Deepimf histAndNbOfSamplesImage;
		const char* inputHistPath_ = inputHistPath.c_str();
		if (!ImageIO::loadMultiChannelsEXR(histAndNbOfSamplesImage, inputHistPath_)){
        	error = "ERROR in program arguments: couldn't load input histogram image file";
			return false;
		}
		Utils::separateNbOfSamplesFromHistogram(programArgs.m_histogramImage, 
      		programArgs.m_nbOfSamplesImage, histAndNbOfSamplesImage);
		std::cout << "Histogram loaded" << "\n";

    	// input covariance path
		const char* inputCovPath = inputCovariancePath.c_str();
    	if (!ImageIO::loadMultiChannelsEXR(programArgs.m_covarianceImage, inputCovPath)){
			error = "ERROR in program arguments: couldn't load input covariance matrix image file";
			return false;
		}
		std::cout << "Covariance loaded" << "\n";

		// check optional parameters
  		if(random_order != 0 && random_order != 1){
			error = "ERROR in program arguments: expecting 0 or 1 after '-r'";
			return false;
		}
  		if(spike_removal != 0 && spike_removal != 1){
			error = "ERROR in program arguments: expecting 0 or 1 after '-p'";
			return false;
		}
		if(skip_marked_patches < 0 || skip_marked_patches > 1){
			error = "ERROR in program arguments: expecting float in [0,1] after '-m'";
			return false;
		}

		// set optional parameters 
		programArgs.m_histogramPatchDistanceThreshold = hist_distance;
  		programArgs.m_markedPixelsSkippingProbability = skip_marked_patches;
  		programArgs.m_patchRadius = patches_radius;
  		programArgs.m_useRandomPixelOrder = random_order;
  		programArgs.m_nbOfScales = multi_scaling;
  		programArgs.m_minEigenValue = min_eigen_val;
  		programArgs.m_nbOfCores = core_to_use;
  		programArgs.m_prefilterThresholdStDevFactor = factor;	
  		programArgs.m_prefilterSpikes = spike_removal;
  		programArgs.m_searchWindowRadius = windows_radius;

		// print info abount optional parameters values
		std::cout << "Histogram distance set to: " << programArgs.m_histogramPatchDistanceThreshold << "\n";
		std::cout << "Windows radius set to: " << programArgs.m_searchWindowRadius << "\n";
		std::cout << "Patch radius set to: " << programArgs.m_patchRadius << "\n";
		std::cout << "Min eigen value set to: " << programArgs.m_minEigenValue << "\n";
		std::cout << "Random pixel order set to: " << programArgs.m_useRandomPixelOrder << "\n";
		std::cout << "Prefilter spikes set to: " << programArgs.m_prefilterSpikes << "\n";
		std::cout << "Std deviation factor set to: " << programArgs.m_prefilterThresholdStDevFactor << "\n";
		std::cout << "Skip probability set to: " << programArgs.m_markedPixelsSkippingProbability << "\n";
		std::cout << "Number of scales set to: " << programArgs.m_nbOfScales << "\n";
		std::cout << "Number of cores set to: " << programArgs.m_nbOfCores << "\n";

		return true;
	}

  void checkAndPutToZeroNegativeInfNaNValues(DeepImage<float>& io_rImage, bool i_verbose = false)
	{
		using std::isnan;
		using std::isinf;
		int width = io_rImage.getWidth();
		int height = io_rImage.getHeight();
		int depth = io_rImage.getDepth();
		bool hasBadValue = false;
		std::vector<float> values(depth);
		float val = 0.f;
		for (int line = 0; line < height; line++)
			for (int col = 0; col < width; col++)
			{
				hasBadValue = false;
				for(int z = 0; z < depth; ++z)
				{
					val = values[z] = io_rImage.get(line, col, z);
					if(val < 0 || isnan(val) || isinf(val))
					{
						io_rImage.set(line, col, z, 0.f);
						hasBadValue = true;
					}
				}
				if(hasBadValue && i_verbose)
				{
					std::cout << "Warning: strange value for pixel (line,column)=(" << line << "," << col << "): (" << values[0];
					for(int i = 1; i < depth; ++i)
						std::cout << ", " << values[i];
					std::cout << ")" << std::endl;
				}
			}
	}

  int launchBayesianCollaborativeDenoising(ProgramArguments_bcd& programArgs, std::string& error)
	{
		if(programArgs.m_prefilterSpikes)
			SpikeRemovalFilter::filter(
					programArgs.m_colorImage,
					programArgs.m_nbOfSamplesImage,
					programArgs.m_histogramImage,
					programArgs.m_covarianceImage,
					programArgs.m_prefilterThresholdStDevFactor);

		DenoiserInputs inputs;
		DenoiserOutputs outputs;
		DenoiserParameters parameters;

		inputs.m_pColors = &(programArgs.m_colorImage);
		inputs.m_pNbOfSamples = &(programArgs.m_nbOfSamplesImage);
		inputs.m_pHistograms = &(programArgs.m_histogramImage);
		inputs.m_pSampleCovariances = &(programArgs.m_covarianceImage);

		Deepimf outputDenoisedColorImage(programArgs.m_colorImage);
		outputs.m_pDenoisedColors = &outputDenoisedColorImage;

		parameters.m_histogramDistanceThreshold = programArgs.m_histogramPatchDistanceThreshold;
		parameters.m_patchRadius = programArgs.m_patchRadius;
		parameters.m_searchWindowRadius = programArgs.m_searchWindowRadius;
		parameters.m_minEigenValue = programArgs.m_minEigenValue;
		parameters.m_useRandomPixelOrder = programArgs.m_useRandomPixelOrder;
		parameters.m_markedPixelsSkippingProbability = programArgs.m_markedPixelsSkippingProbability;
		parameters.m_nbOfCores = programArgs.m_nbOfCores;

		std::unique_ptr<IDenoiser> uDenoiser = nullptr;

		if(programArgs.m_nbOfScales > 1)
			uDenoiser.reset(new MultiscaleDenoiser(programArgs.m_nbOfScales));
		else
			uDenoiser.reset(new Denoiser());

		uDenoiser->setInputs(inputs);
		uDenoiser->setOutputs(outputs);
		uDenoiser->setParameters(parameters);

		uDenoiser->denoise();

		checkAndPutToZeroNegativeInfNaNValues(outputDenoisedColorImage);

		ImageIO::writeEXR(outputDenoisedColorImage, programArgs.m_denoisedOutputFilePath.c_str());
		std::cout << "Written denoised output in file " << programArgs.m_denoisedOutputFilePath.c_str() << std::endl;

		return 0;
	} // end bcd

} // namespace bcd


int main(int argc, const char* argv[]) {
  // command line parameters
  auto output = "out.png"s;
  auto input = "img.hdr"s;
  auto hist_distance = 1.f;
  auto windows_radius = 6;
  auto patches_radius = 1;
  auto random_order = 1; 
  auto spike_removal = 1; 
  auto factor = 2.f;
  auto skip_marked_patches = 1.f;
  auto multi_scaling = 3;
  auto core_to_use = 0;
  auto min_eigen_val = 1.e-8f;

  // parse command line
  auto cli = cli::make_cli("yimgdenoise", "Denoise images");
  add_option(cli, "--output,-o", output, "output image filename", true);
  add_option(cli, "filename", input, "input image filename", true);
  add_option(cli, "-d", hist_distance, "histogram patch distance threshold");
  add_option(cli, "-b", windows_radius, "radius of search windows");
  add_option(cli, "-w", patches_radius, "radius of patches");
  add_option(cli, "-r", random_order, "1 for random pixel order");
  add_option(cli, "-p", spike_removal, "1 for spike removal prefiltering");
  add_option(cli, "--p-factor", factor, "factor that is multiplied by standard deviation to get the threshold for classifying spikes during prefiltering. Put lower value to remove more spikes");
  add_option(cli, "-m", skip_marked_patches, "probability of skipping marked centers of denoised patches. 1 accelerates a lot the computations. 0 helps removing potential grid artifacts");
  add_option(cli, "-s", multi_scaling, "number of Scales for Multi-Scaling");
  add_option(cli, "--ncores", core_to_use, "number of cores used by OpenMP");
  add_option(cli, "-e", min_eigen_val, "minimum eigen value for matrix inversion");
  parse_cli(cli, argc, argv);
  
  auto error  = ""s;

  //raw2bcd
  int rc = bcd::convertRawToBcd(input, output, error);
  if(rc == 1)
    cli::print_fatal(error);

  std::cout << "Conversion done" << "\n";
  
  //bcd 
  bcd::ProgramArguments_bcd programArgs_;
  if(!bcd::parseBcdArguments(programArgs_, error, output, hist_distance, windows_radius, patches_radius, random_order,
  							spike_removal, factor, skip_marked_patches, multi_scaling, core_to_use, min_eigen_val))
	cli::print_fatal(error);

  rc = bcd::launchBayesianCollaborativeDenoising(programArgs_, error);
  if(rc == 1)
    cli::print_fatal(error);

  std::cout << "Image denoised!" << std::endl;

  // done
  return rc;
}