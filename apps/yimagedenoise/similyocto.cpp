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

using namespace yocto::math;
namespace img = yocto::image;
namespace cli = yocto::commonio;
namespace gui = yocto::gui;


using namespace std;

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
		string m_inputFilePath; ///< File path to the input image
		string m_outputColorFilePath; ///< File path to the output color image
		string m_outputHistogramFilePath; ///< File path to the output histogram image
		string m_outputCovarianceFilePath; ///< File path to the output covariance image
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

  bool parseProgramArguments_rawToBcd(string inputFilePath, string outputFilePathPrefix,
   ProgramArguments_raw2bcd& o_rProgramArguments, std::string& error)
  {
    o_rProgramArguments.m_inputFilePath = inputFilePath;
    if(!ifstream(o_rProgramArguments.m_inputFilePath)){
      error = "ERROR in program arguments: cannot open input file";
      return false;
    }

		o_rProgramArguments.m_outputColorFilePath = outputFilePathPrefix + g_pColorSuffix + g_pDeepImageExtension;
		o_rProgramArguments.m_outputHistogramFilePath = outputFilePathPrefix + g_pHistogramSuffix + g_pDeepImageExtension;
		o_rProgramArguments.m_outputCovarianceFilePath = outputFilePathPrefix + g_pCovarianceSuffix + g_pDeepImageExtension;

    if(!ofstream(o_rProgramArguments.m_outputColorFilePath)){
      error = "ERROR in program arguments: cannot write output file for color";
      return false;
    }
    if(!ofstream(o_rProgramArguments.m_outputHistogramFilePath)){
      error = "ERROR in program arguments: cannot write output file for histogram";
      return false;
    }
    if(!ofstream(o_rProgramArguments.m_outputCovarianceFilePath)){
      error = "ERROR in program arguments: cannot write output file for covariance";
      return false;
    }
       
    return true;
  }

  void printHeader_raw2bcd(const RawFileHeader& i_rHeader)
	{
		cout << "Version: " << i_rHeader.version << endl;
		cout << "Resolution: " << i_rHeader.width << "x" << i_rHeader.height << endl;
		cout << "Nb of samples: " << i_rHeader.nbOfSamples << endl;
		cout << "Nb of channels: " << i_rHeader.nbOfChannels << endl;
	}

  int convertRawToBcd(string inputFilePath, string outputFilePathPrefix, std::string& error)
	{
		ProgramArguments_raw2bcd programArgs;
		if(!parseProgramArguments_rawToBcd(inputFilePath, outputFilePathPrefix, programArgs, error))
			return 1;

		ifstream inputFile(programArgs.m_inputFilePath.c_str(), ios::binary);
		RawFileHeader header;
		inputFile.read(reinterpret_cast<char*>(&header), sizeof(RawFileHeader));
		printHeader_raw2bcd(header);

		HistogramParameters histoParams;
		histoParams.m_nbOfBins = programArgs.m_histogramNbOfBins;
		histoParams.m_gamma = programArgs.m_histogramGamma;
		histoParams.m_maxValue = programArgs.m_histogramMaxValue;
		SamplesAccumulator samplesAccumulator(header.width, header.height, histoParams);

		float sample[4]; // assumes nbOfChannels can be only 3 or 4
		streamsize sampleSize = header.nbOfChannels * sizeof(float);
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
			m_nbOfCores(0),
			m_useCuda(true)
		{}

	public:
		string m_denoisedOutputFilePath; ///< File path to the denoised image output
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
		bool m_useCuda; ///< True means that the program will use Cuda (if available) to parallelize computations
	};

  void initializeRandomSeed()
	{
		srand(static_cast<unsigned int>(time(0)));
	}

  bool parseProgramArguments_bcd(string outputPath, const char *inputColorPath, const char *inputHistPath, 
    const char *inputCovariancePath, ProgramArguments_bcd& o_rProgramArguments, std::string& error)
	{
    // output path
    o_rProgramArguments.m_denoisedOutputFilePath = outputPath;
    ofstream outputFile(o_rProgramArguments.m_denoisedOutputFilePath, ofstream::out | ofstream::app);
		if(!outputFile){
			error =  "ERROR in program arguments: cannot write output file";
			return false;
		}

    // input color path
    string inputColorFilePath = inputColorPath;
    if (!ImageIO::loadEXR(o_rProgramArguments.m_colorImage, inputColorPath)){
			error = "ERROR in program arguments: couldn't load input color image file";
			return false;
		}

    // input histogram path
    Deepimf histAndNbOfSamplesImage;
		if (!ImageIO::loadMultiChannelsEXR(histAndNbOfSamplesImage, inputHistPath)){
      error = "ERROR in program arguments: couldn't load input histogram image file";
			return false;
		}
		Utils::separateNbOfSamplesFromHistogram(o_rProgramArguments.m_histogramImage, 
      o_rProgramArguments.m_nbOfSamplesImage, histAndNbOfSamplesImage);

    // input covariance path
    if (!ImageIO::loadMultiChannelsEXR(o_rProgramArguments.m_covarianceImage, inputCovariancePath)){
			error = "ERROR in program arguments: couldn't load input covariance matrix image file";
			return false;
		}

    //TODO: manca la gestione dei parametri opzionali
    /*
    else if (strcmp(argv[argIndex], "-d") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting histogram patch distance threshold after '-d'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_histogramPatchDistanceThreshold;
			}
			else if (strcmp(argv[argIndex], "-b") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting radius of search window after '-b'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_searchWindowRadius;
			}
			else if (strcmp(argv[argIndex], "-w") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting radius of patch after '-w'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_patchRadius;
			}
			else if (strcmp(argv[argIndex], "-e") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting minimum eigen value after '-e'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_minEigenValue;
			}
			else if (strcmp(argv[argIndex], "-r") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting 0 or 1 after '-r'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				int useRandomPixelOrder;
				iss >> useRandomPixelOrder;
				if(useRandomPixelOrder != 0 && useRandomPixelOrder != 1)
				{
					cout << "ERROR in program arguments: expecting 0 or 1 after '-r'" << endl;
					return false;
				}
				o_rProgramArguments.m_useRandomPixelOrder = (useRandomPixelOrder==1);
			}
			else if (strcmp(argv[argIndex], "-p") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting 0 or 1 after '-p'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				int prefilterSpikes;
				iss >> prefilterSpikes;
				if(prefilterSpikes != 0 && prefilterSpikes != 1)
				{
					cout << "ERROR in program arguments: expecting 0 or 1 after '-p'" << endl;
					return false;
				}
				o_rProgramArguments.m_prefilterSpikes = (prefilterSpikes==1);
			}
			else if (strcmp(argv[argIndex], "--p-factor") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting standard deviation factor for spike prefiltering threshold after '--p-factor'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_prefilterThresholdStDevFactor;
			}
			else if(strcmp(argv[argIndex], "-m") == 0)
			{
				argIndex++;
				if(argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting float in [0,1] after '-m'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				float markedPixelsSkippingProbability;
				iss >> markedPixelsSkippingProbability;
				if(markedPixelsSkippingProbability < 0 || markedPixelsSkippingProbability > 1)
				{
					cout << "ERROR in program arguments: expecting float in [0,1] after '-m'" << endl;
					return false;
				}
				o_rProgramArguments.m_markedPixelsSkippingProbability = markedPixelsSkippingProbability;
			}
			else if(strcmp(argv[argIndex], "-s") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting number of scales after '-s'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_nbOfScales;
			}
			else if (strcmp(argv[argIndex], "--ncores") == 0)
			{
				argIndex++;
				if (argIndex == argc)
				{
					cout << "ERROR in program arguments: expecting number of cores for OpenMP after '--ncores'" << endl;
					return false;
				}
				istringstream iss(argv[argIndex]);
				iss >> o_rProgramArguments.m_nbOfCores;
			}
		} */
    
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
		vector<float> values(depth);
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
					cout << "Warning: strange value for pixel (line,column)=(" << line << "," << col << "): (" << values[0];
					for(int i = 1; i < depth; ++i)
						cout << ", " << values[i];
					cout << ")" << endl;
				}
			}
	}

  int launchBayesianCollaborativeDenoising(string outputPath, const char *inputColorPath, const char *inputHistPath, 
    const char *inputCovariancePath, std::string& error)
	{
		ProgramArguments_bcd programArgs;
		if(!parseProgramArguments_bcd(outputPath, inputColorPath, inputHistPath, inputCovariancePath, programArgs, error))
			return 1;

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
		parameters.m_useCuda = programArgs.m_useCuda;

		unique_ptr<IDenoiser> uDenoiser = nullptr;

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
		cout << "Written denoised output in file " << programArgs.m_denoisedOutputFilePath.c_str() << endl;

		return 0;
	} // end bcd

  void pauseBeforeExit()
	{
		cout << "Exiting program!" << endl;
	}

} // namespace bcd


int main(int argc, const char* argv[]) {
  // command line parameters
  auto output = "out.png"s;
  auto input = "img.hdr"s;

  // parse command line
  auto cli = cli::make_cli("yimgdenoise", "Denoise images");
  add_option(cli, "--output,-o", output, "output image filename");
  add_option(cli, "filename", input, "input image filename", true);
  parse_cli(cli, argc, argv);
  
  // error std::string buffer
  auto error = ""s;

  // load
  //auto ext      = sfs::path(input).extension().string();
  //auto basename = sfs::path(input).stem().string();
  
  auto ioerror  = ""s;
  
  bcd::Chronometer programTotalTime;
	programTotalTime.start();

  //raw2bcd
  int rc = bcd::convertRawToBcd(output, input, ioerror);

  bcd::pauseBeforeExit();

  if(rc == 1)
    cli::print_fatal(ioerror);

  
  //bcd 
  

	bcd::g_pProgramPath = argv[0]; // N.B.:questo mi sa che va cambiato
	int rc; // return code

	// tocca ridefinirlo senza argc e argv:
  //rc = bcd::launchBayesianCollaborativeDenoising(argc, argv);

	programTotalTime.stop();
	cout << "Program total time: ";
	programTotalTime.printElapsedTime();
	cout << endl;

	bcd::pauseBeforeExit();
	return rc;

  // done
  return 0;
}