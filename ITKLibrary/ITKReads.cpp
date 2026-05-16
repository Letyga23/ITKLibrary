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
#include "dcmtk/dcmdata/dctk.h"
#include "dcmtk/dcmimgle/dcmimage.h"

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

template<typename PixelType>
bool ReadSeriesWithITK(const char** series, int fileCount, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
{
	using ImageType = itk::Image<PixelType, 3>;
	using ReaderType = itk::ImageSeriesReader<ImageType>;
	using ImageIOType = itk::GDCMImageIO;
	using NamesGeneratorType = itk::GDCMSeriesFileNames;

	try
	{
		if (fileCount == 0 || series == nullptr || series[0] == nullptr)
			return false;

		std::string firstFile = series[0];
		size_t lastSlash = firstFile.find_last_of("/\\");
		std::string folder = (lastSlash != std::string::npos) ? firstFile.substr(0, lastSlash) : firstFile;

		typename NamesGeneratorType::Pointer nameGen = NamesGeneratorType::New();
		nameGen->SetUseSeriesDetails(true);
		nameGen->AddSeriesRestriction("0020|000E");
		nameGen->SetDirectory(folder);

		typename ReaderType::FileNamesContainer sortedNames = nameGen->GetInputFileNames();

		if (sortedNames.empty() && fileCount > 0)
		{
			sortedNames.resize(fileCount);
			for (int i = 0; i < fileCount; ++i)
				sortedNames[i] = series[i];
		}

		if (sortedNames.empty())
		{
			OutputDebugStringA("No DICOM files found.\n");
			return false;
		}

		typename ReaderType::Pointer reader = ReaderType::New();
		ImageIOType::Pointer dicomIO = ImageIOType::New();

		reader->SetImageIO(dicomIO);
		reader->SetFileNames(sortedNames);
		reader->ForceOrthogonalDirectionOff();

		reader->Update();

		typename ImageType::Pointer image = reader->GetOutput();
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
		std::string msg = "ITK Exception: " + std::string(e.what()) + "\n";
		OutputDebugStringA(msg.c_str());
		return false;
	}
	catch (std::exception& e)
	{
		std::string msg = "Exception: " + std::string(e.what()) + "\n";
		OutputDebugStringA(msg.c_str());
		return false;
	}
}

bool ReadMultiFrameWithDCMTK(DcmDataset* ds, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
{
	Uint16 rows = 0, cols = 0, bitsAllocated = 0, pixelRep = 0;
	Uint32 frames = 1;

	ds->findAndGetUint16(DCM_Rows, rows);
	ds->findAndGetUint16(DCM_Columns, cols);
	ds->findAndGetUint16(DCM_BitsAllocated, bitsAllocated);
	ds->findAndGetUint16(DCM_PixelRepresentation, pixelRep);
	ds->findAndGetUint32(DCM_NumberOfFrames, frames);

	if (rows == 0 || cols == 0 || frames == 0)
		return false;

	double spacingX = 1.0;
	double spacingY = 1.0;
	double spacingZ = 1.0;

	OFString pixelSpacingStr;
	if (ds->findAndGetOFString(DCM_PixelSpacing, pixelSpacingStr).good() && !pixelSpacingStr.empty())
	{
		double valY = 1.0, valX = 1.0;
		if (sscanf_s(pixelSpacingStr.c_str(), "%lf\\%lf", &valY, &valX) == 2)
		{
			spacingX = valX;
			spacingY = valY;
		}
	}

	double spacingBetweenSlices = 0.0;
	double sliceThickness = 0.0;

	if (ds->findAndGetFloat64(DCM_SpacingBetweenSlices, spacingBetweenSlices).good() && spacingBetweenSlices > 0.0)
		spacingZ = spacingBetweenSlices;
	else if (ds->findAndGetFloat64(DCM_SliceThickness, sliceThickness).good() && sliceThickness > 0.0)
		spacingZ = sliceThickness;

	double slope = 1.0, intercept = 0.0;
	ds->findAndGetFloat64(DCM_RescaleSlope, slope);
	ds->findAndGetFloat64(DCM_RescaleIntercept, intercept);

	const Uint16* pixelData = nullptr;
	if (!ds->findAndGetUint16Array(DCM_PixelData, pixelData).good() || !pixelData)
		return false;

	const size_t voxelCount = static_cast<size_t>(cols) * rows * frames;
	const size_t totalBytes = voxelCount * sizeof(float);

	*outBuffer = new uint8_t[totalBytes];
	*outSize = totalBytes;

	float* dest = reinterpret_cast<float*>(*outBuffer);

	for (size_t i = 0; i < voxelCount; ++i)
	{
		double val = (pixelRep == 0) ? static_cast<double>(pixelData[i]) : static_cast<double>(reinterpret_cast<const int16_t*>(pixelData)[i]);
		dest[i] = static_cast<float>(val * slope + intercept);
	}

	if (outInfo)
	{
		outInfo->DimX = cols;
		outInfo->DimY = rows;
		outInfo->DimZ = frames;
		outInfo->SpacingX = static_cast<float>(spacingX);
		outInfo->SpacingY = static_cast<float>(spacingY);
		outInfo->SpacingZ = static_cast<float>(spacingZ);
		outInfo->BytesPerVoxel = sizeof(float);
		outInfo->bIsSigned = true;
	}

	return true;
}

std::string Utf8ToAnsi(const std::string& utf8Str) 
{
	if (utf8Str.empty()) return "";

	int wsize = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
	std::wstring wstr(wsize, 0);
	MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], wsize);

	int asize = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
	std::string ansiStr(asize, 0);
	WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &ansiStr[0], asize, NULL, NULL);

	if (!ansiStr.empty() && ansiStr.back() == '\0')
		ansiStr.pop_back();

	return ansiStr;
}

extern "C" __declspec(dllexport)
bool ReadDicomSeriesToVolume(const char** series, int fileCount, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
{
	try
	{
		if (fileCount == 0 || series == nullptr || series[0] == nullptr)
			return false;

		std::string firstFile = series[0];
		std::string firstFileAnsi = Utf8ToAnsi(firstFile);

		DcmFileFormat dcmFile;
		OFCondition status = dcmFile.loadFile(firstFileAnsi.c_str());
		if (!status.good())
		{
			OutputDebugStringA("Failed to load DICOM file with DCMTK\n");
			return false;
		}

		DcmDataset* ds = dcmFile.getDataset();
		if (!ds)
			return false;

		Uint32 numberOfFrames = 1;
		ds->findAndGetUint32(DCM_NumberOfFrames, numberOfFrames);

		bool isMultiFrame = (numberOfFrames > 1);
		bool isCT = false;

		OFString modality;
		if (ds->findAndGetOFString(DCM_Modality, modality).good())
		{
			if (modality == "CT")
				isCT = true;
		}

		if (isMultiFrame)
			return ReadMultiFrameWithDCMTK(ds, outBuffer, outSize, outInfo);
		else
		{
			if (isCT)
				return ReadSeriesWithITK<signed short>(series, fileCount, outBuffer, outSize, outInfo);
			else
				return ReadSeriesWithITK<float>(series, fileCount, outBuffer, outSize, outInfo);
		}
	}
	catch (const itk::ExceptionObject& e)
	{
		OutputDebugStringA(("ITK Exception: " + std::string(e.GetDescription()) + "\n").c_str());
		return false;
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA(("Exception: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}

extern "C" __declspec(dllexport)
void FreeVolumeBuffer(uint8_t * buffer)
{
	delete[] buffer;
}