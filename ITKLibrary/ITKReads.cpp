#include "pch.h"
#include "itkImageSeriesReader.h"
#include "itkScalarToRGBPixelFunctor.h"
#include "itkMetaDataDictionary.h"
#include "itkMetaDataObject.h"
#include "itkNumericTraits.h"
#include "itkImageIOBase.h"
#include "itkImageIORegion.h"
#include "itkDCMTKImageIO.h"
#include "itkDCMTKSeriesFileNames.h"
#include "itkDCMTKFileReader.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "gdcmFileDecompressLookupTable.h"
#include "itkIntensityWindowingImageFilter.h"
#include "DICOMParser.h"
#include "itkOpenCVImageBridge.h"

#include <vector>
#include <codecvt>

extern "C" __declspec(dllexport) void ITKReadDCM(int Slice, signed short& WL, signed short& WW, std::string & FolderName, cv::Mat & Image)
{
	using PixelType = signed short;
	constexpr unsigned int Dimension = 3; // The dimension is 3, not 2
	using ImageType = itk::Image<PixelType, Dimension>;
	using ImageIOType = itk::DCMTKImageIO;
	using IntensityWindowingImageFilterType = itk::IntensityWindowingImageFilter<ImageType, ImageType>;
	itk::DCMTKImageIO::Pointer DicomIO = itk::DCMTKImageIO::New();
	itk::DCMTKSeriesFileNames::Pointer nameGenarator = itk::DCMTKSeriesFileNames::New();
	nameGenarator->SetDirectory(FolderName);
	using FilenamesContainer = std::vector<std::string>;
	FilenamesContainer FileNames = nameGenarator->GetInputFileNames();
	cv::Mat img;
	using ReaderType = itk::ImageSeriesReader<ImageType>;
	ReaderType::Pointer reader = ReaderType::New();
	
	auto filter = IntensityWindowingImageFilterType::New();
	auto var = FileNames[Slice];
	
	reader->SetFileName(var);
	reader->SetImageIO(DicomIO);
	reader->Update();
	filter->SetInput(reader->GetOutput());
	filter->SetWindowMinimum(0);
	filter->SetWindowMaximum(100);
	filter->SetWindowLevel(WW, WL);
	filter->SetOutputMinimum(0);
	filter->SetOutputMaximum(255);
	filter->Update();
	img = itk::OpenCVImageBridge::ITKImageToCVMat<ImageType>(filter->GetOutput());
	img.convertTo(Image, CV_8U);

}

extern "C" __declspec(dllexport) void ITKInitializeDCM(std::string & FolderName, std::vector <cv::Mat> & Images)
{
	using PixelType = signed short;
	constexpr unsigned int Dimension = 3; // The dimension is 3, not 2
	using ImageType = itk::Image<PixelType, Dimension>;
	using ImageIOType = itk::DCMTKImageIO;
	using IntensityWindowingImageFilterType = itk::IntensityWindowingImageFilter<ImageType, ImageType>;
	itk::DCMTKImageIO::Pointer DicomIO = itk::DCMTKImageIO::New();
	itk::DCMTKSeriesFileNames::Pointer nameGenarator = itk::DCMTKSeriesFileNames::New();
	nameGenarator->SetDirectory(FolderName);
	using FilenamesContainer = std::vector<std::string>;
	FilenamesContainer FileNames = nameGenarator->GetInputFileNames();
	cv::Mat img;
	cv::Mat dst;
	using ReaderType = itk::ImageSeriesReader<ImageType>;
	ReaderType::Pointer reader = ReaderType::New();

	auto filter = IntensityWindowingImageFilterType::New();
	for (auto var : FileNames)
	{
		reader->SetFileName(var);
		reader->SetImageIO(DicomIO);
		reader->Update();
		filter->SetInput(reader->GetOutput());
		filter->SetWindowMinimum(0);
		filter->SetWindowMaximum(100);
		filter->SetWindowLevel(450, 45);
		filter->SetOutputMinimum(0);
		filter->SetOutputMaximum(255);
		filter->Update();
		img = itk::OpenCVImageBridge::ITKImageToCVMat<ImageType>(filter->GetOutput());
		img.convertTo(dst, CV_8U);
		Images.push_back(dst);
	}
}

extern "C" __declspec(dllexport) void ITKFileNames(std::string & FolderName, std::vector<std::string>& FileNames)
{
	FileNames.clear();
	using PixelType = signed short;
	constexpr unsigned int Dimension = 3; // The dimension is 3, not 2

	using ImageType = itk::Image<PixelType, Dimension>;

	itk::DCMTKSeriesFileNames::Pointer nameGenarator = itk::DCMTKSeriesFileNames::New();
	nameGenarator->SetDirectory(FolderName);
	if (nameGenarator.IsNotNull())
	{
		FileNames = nameGenarator->GetInputFileNames();
	}
}

extern "C" __declspec(dllexport) void ITKPatientInfo(std::string & FolderName, std::string & bodyPart, std::string& modality, std::string & patientID)
{
	typedef signed short InternalPixelType;
	const unsigned int Dimension = 3; // The dimension is 3, not 2
	using InternalImageType = itk::Image<InternalPixelType, Dimension>;

	typedef itk::Image<InternalPixelType, Dimension> ImageType;
	typedef itk::ImageSeriesReader<ImageType> ReaderType;

	typedef itk::GDCMImageIO ImageIOType;
	typedef itk::DCMTKSeriesFileNames NamesGeneratorType;

	ImageIOType::Pointer gdcmIO = ImageIOType::New();
	NamesGeneratorType::Pointer namesGenerator = NamesGeneratorType::New();

	namesGenerator->SetInputDirectory(FolderName);

	const ReaderType::FileNamesContainer& filenames =
		namesGenerator->GetInputFileNames();

	std::size_t numberOfFileNames = filenames.size();
	std::cout << numberOfFileNames << std::endl;


	ReaderType::Pointer reader = ReaderType::New();
	reader->SetImageIO(gdcmIO);
	reader->SetFileNames(filenames);


	// Reading Data;
	try
	{
		reader->Update();
		reader->GetMetaDataDictionary();
		gdcmIO->GetMetaDataDictionary();// Read header file information;

		//Information assignment
		char* name = new char[50];
		char* _patientID = new char[50];
		char* time = new char[50];
		char* manufacture = new char[50];
		char* _modality = new char[50];
		char* hospital = new char[50];
		char* sex = new char[50];
		char* age = new char[50];
		char* description = new char[100];
		char* _bodyPart = new char[50];

		ImageIOType::ByteOrder byteOrder;
		byteOrder = gdcmIO->GetByteOrder();

		unsigned int dim = 0;
		gdcmIO->GetDimensions(dim);
		ImageIOType::SizeType imgsize;
		imgsize = gdcmIO->GetImageSizeInPixels();
		int componetSize = gdcmIO->GetComponentSize();
		int dimension = gdcmIO->GetNumberOfDimensions();
		int ori = 0;

		gdcmIO->GetOrigin(ori);
		int spa = 0;

		gdcmIO->GetPatientSex(sex);
		gdcmIO->GetPatientAge(age);
		gdcmIO->GetStudyDescription(description);
		gdcmIO->GetSpacing(spa);
		gdcmIO->GetPatientName(name);
		gdcmIO->GetModality(_modality);
		gdcmIO->GetPatientID(_patientID);
		gdcmIO->GetManufacturer(manufacture);
		gdcmIO->GetStudyDate(time);
		gdcmIO->GetBodyPart(_bodyPart);
		
		gdcmIO->GetInstitution(hospital);
		ImageType::SpacingType spacetype;
		spacetype = reader->GetOutput()->GetSpacing();
		ImageType::PointType origin;
		origin = reader->GetOutput()->GetOrigin();

		bodyPart = _bodyPart;
		modality = _modality;
		patientID = _patientID;
	}
	catch (const itk::ExceptionObject& excp)
	{
		std::cout << " Reading Exceptaion Caught" << std::endl;
		std::cout << excp.what() << std::endl;
	}
}

struct HDVolumeInfo
{
	int DimX;
	int DimY;
	int DimZ;
	float SpacingX;
	float SpacingY;
	float SpacingZ;
	int BytesPerVoxel;
	bool bIsSigned;
};

//extern "C" __declspec(dllexport)
//bool ReadDicomSeriesToVolume(const char** series, int fileCount, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
//{
//	using PixelType = signed short;
//	using ImageType = itk::Image<PixelType, 3>;
//	using ReaderType = itk::ImageSeriesReader<ImageType>;
//	using ImageIOType = itk::GDCMImageIO;
//
//	try
//	{
//		ReaderType::Pointer reader = ReaderType::New();
//		ImageIOType::Pointer dicomIO = ImageIOType::New();
//		reader->SetImageIO(dicomIO);
//
//		ReaderType::FileNamesContainer fileNames;
//		fileNames.reserve(fileCount);
//		for (int i = 0; i < fileCount; ++i)
//		{
//			fileNames.push_back(series[i]); // уже UTF-8, конвертация не нужна
//		}
//
//		reader->SetFileNames(fileNames);
//		reader->Update();
//
//		ImageType::Pointer image = reader->GetOutput();
//		auto region = image->GetLargestPossibleRegion();
//		auto size = region.GetSize();
//		auto spacing = image->GetSpacing();
//
//		const size_t voxelCount = static_cast<size_t>(size[0]) * size[1] * size[2];
//		const size_t totalBytes = voxelCount * sizeof(PixelType);
//
//		*outBuffer = new uint8_t[totalBytes];
//		*outSize = totalBytes;
//		memcpy(*outBuffer, image->GetBufferPointer(), totalBytes);
//
//		if (outInfo)
//		{
//			outInfo->DimX = static_cast<int>(size[0]);
//			outInfo->DimY = static_cast<int>(size[1]);
//			outInfo->DimZ = static_cast<int>(size[2]);
//			outInfo->SpacingX = static_cast<float>(spacing[0]);
//			outInfo->SpacingY = static_cast<float>(spacing[1]);
//			outInfo->SpacingZ = static_cast<float>(spacing[2]);
//			outInfo->BytesPerVoxel = sizeof(PixelType);
//			outInfo->bIsSigned = true;
//		}
//
//		return true;
//	}
//	catch (itk::ExceptionObject& e)
//	{
//		OutputDebugStringA(("ITK Exception: " + std::string(e.what()) + "\n").c_str());
//		return false;
//	}
//}


extern "C" __declspec(dllexport)
bool ReadDicomSeriesToVolume(
	const char** series,
	int fileCount,
	uint8_t** outBuffer,
	size_t* outSize,
	HDVolumeInfo* outInfo)
{
	using PixelType = signed short;
	using ImageType = itk::Image<PixelType, 3>;
	using ReaderType = itk::ImageSeriesReader<ImageType>;
	using ImageIOType = itk::GDCMImageIO;
	using NamesGeneratorType = itk::GDCMSeriesFileNames;

	try
	{
		std::string firstFile = series[0];
		size_t lastSlash = firstFile.find_last_of("/\\");
		std::string folder = (lastSlash != std::string::npos) ? firstFile.substr(0, lastSlash) : firstFile;

		NamesGeneratorType::Pointer nameGen = NamesGeneratorType::New();
		nameGen->SetUseSeriesDetails(true);
		nameGen->AddSeriesRestriction("0008|0021");
		nameGen->SetDirectory(folder);

		const ReaderType::FileNamesContainer sortedNames = nameGen->GetInputFileNames();

		if (sortedNames.empty())
		{
			OutputDebugStringA("No DICOM files found or failed to read metadata.\n");
			return false;
		}

		ReaderType::Pointer reader = ReaderType::New();
		ImageIOType::Pointer dicomIO = ImageIOType::New();
		reader->SetImageIO(dicomIO);
		reader->SetFileNames(sortedNames);
		reader->Update();

		ImageType::Pointer image = reader->GetOutput();
		auto region = image->GetLargestPossibleRegion();
		auto size = region.GetSize();
		auto spacing = image->GetSpacing();

		const size_t voxelCount = static_cast<size_t>(size[0]) * size[1] * size[2];
		const size_t totalBytes = voxelCount * sizeof(PixelType);

		*outBuffer = new uint8_t[totalBytes];
		*outSize = totalBytes;
		memcpy(*outBuffer, image->GetBufferPointer(), totalBytes);

		if (outInfo)
		{
			outInfo->DimX = static_cast<int>(size[0]);
			outInfo->DimY = static_cast<int>(size[1]);
			outInfo->DimZ = static_cast<int>(size[2]);
			outInfo->SpacingX = static_cast<float>(spacing[0]);
			outInfo->SpacingY = static_cast<float>(spacing[1]);
			outInfo->SpacingZ = static_cast<float>(spacing[2]);
			outInfo->BytesPerVoxel = sizeof(PixelType);
			outInfo->bIsSigned = true;
		}

		return true;
	}
	catch (itk::ExceptionObject& e)
	{
		OutputDebugStringA(("ITK Exception: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}

extern "C" __declspec(dllexport)
void FreeVolumeBuffer(uint8_t * buffer)
{
	delete[] buffer;
}