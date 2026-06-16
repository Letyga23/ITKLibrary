#include "itkImageSeriesReader.h"
#include "itkMetaDataDictionary.h"
#include "itkNumericTraits.h"
#include "itkImageIOBase.h"
#include "itkImageIORegion.h"
#include "itkDCMTKSeriesFileNames.h"
#include "itkDCMTKFileReader.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkIntensityWindowingImageFilter.h"
#include "dcmtk/dcmdata/dctk.h"
#include <itkOrientImageFilter.h>
#include <itkSpatialOrientation.h>
#include <itkCastImageFilter.h>

#include <vector>

inline void LogInfo(const std::string& msg)
{
	std::cerr << "[INFO] " << msg << std::endl;
}

inline void LogError(const std::string& msg)
{
	std::cerr << "[ERROR] " << msg << std::endl;
}

std::string Utf8ToAnsi(const std::string& utf8Str)
{
#ifdef _WIN32
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
#else
	return utf8Str;
#endif
}

struct DcmTagKeyHash {
	std::size_t operator()(const DcmTagKey& k) const {
		return (static_cast<std::size_t>(k.getGroup()) << 16) | k.getElement();
	}
};

using FrameDataMapDCMTK = std::unordered_map<DcmTagKey, std::string, DcmTagKeyHash>;

bool FindTagRecursiveUint32(DcmItem* item, const DcmTagKey& target, Uint32& outValue)
{
	if (!item) return false;

	if (item->findAndGetUint32(target, outValue, 0, OFFalse).good())
		return true;

	unsigned long numElements = item->card();
	for (unsigned long i = 0; i < numElements; ++i)
	{
		DcmElement* elem = item->getElement(i);
		if (elem && elem->ident() == EVR_SQ)
		{
			DcmSequenceOfItems* sq = reinterpret_cast<DcmSequenceOfItems*>(elem);
			unsigned long numItems = sq->card();
			for (unsigned long j = 0; j < numItems; ++j)
			{
				if (FindTagRecursiveUint32(sq->getItem(j), target, outValue))
					return true;
			}
		}
	}
	return false;
}

bool BuildFramesDataDCMTK(DcmDataset* ds, std::vector<FrameDataMapDCMTK>& outFramesData)
{
	if (!ds) return false;

	DcmTagKey perFrameTag(0x5200, 0x9230);
	DcmElement* element = nullptr;
	if (!ds->findAndGetElement(perFrameTag, element).good() || !element)
		return false;

	if (element->ident() != EVR_SQ)
		return false;

	DcmSequenceOfItems* perSeq = reinterpret_cast<DcmSequenceOfItems*>(element);
	unsigned long numFrames = perSeq->card();
	if (numFrames == 0)
		return false;

	outFramesData.resize(numFrames);

	const std::vector<DcmTagKey> neededTags = {
		DCM_PixelData,
		DCM_NumberOfFrames,
		DCM_RescaleSlope,
		DCM_RescaleIntercept,
		DCM_SpacingBetweenSlices,
		DCM_SliceThickness,
		DCM_ImagePositionPatient,
		DCM_PixelSpacing,
		DCM_Rows,
		DCM_Columns,
		DCM_BitsAllocated,
		DCM_PixelRepresentation
	};

	auto extractLambda = [](DcmItem* item, const std::vector<DcmTagKey>& tags, FrameDataMapDCMTK& outMap, auto& self) -> void {
		for (const auto& tag : tags) {
			if (outMap.find(tag) != outMap.end()) continue;
			OFString ofStr;
			if (item->findAndGetOFString(tag, ofStr, 0, OFFalse).good()) {
				std::string value(ofStr.c_str());
				value.erase(std::remove(value.begin(), value.end(), '\0'), value.end());
				outMap[tag] = value;
			}
		}
		if (outMap.size() == tags.size()) return;

		unsigned long numElems = item->card();
		for (unsigned long i = 0; i < numElems; ++i) {
			DcmElement* elem = item->getElement(i);
			if (elem && elem->ident() == EVR_SQ) {
				DcmSequenceOfItems* sq = reinterpret_cast<DcmSequenceOfItems*>(elem);
				unsigned long numItems = sq->card();
				for (unsigned long j = 0; j < numItems; ++j) {
					self(sq->getItem(j), tags, outMap, self);
				}
			}
		}
		};

	for (unsigned long i = 0; i < numFrames; ++i)
	{
		DcmItem* frameItem = perSeq->getItem(i);
		if (frameItem) {
			extractLambda(frameItem, neededTags, outFramesData[i], extractLambda);
		}
	}
	return true;
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
bool ReadSeriesWithITK(std::string firstFile, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
{
	using ImageType = itk::Image<PixelType, 3>;
	using ReaderType = itk::ImageSeriesReader<ImageType>;
	using ImageIOType = itk::GDCMImageIO;
	using NamesGeneratorType = itk::GDCMSeriesFileNames;

	try
	{
		if (firstFile.empty())
			return false;

		size_t lastSlash = firstFile.find_last_of("/\\");
		std::string folder = (lastSlash != std::string::npos) ? firstFile.substr(0, lastSlash) : firstFile;

		typename NamesGeneratorType::Pointer nameGen = NamesGeneratorType::New();
		nameGen->SetUseSeriesDetails(true);
		nameGen->AddSeriesRestriction("0020|000E");
		nameGen->SetDirectory(folder);

		std::string targetUID = "";
		{
			std::string firstFileAnsi = Utf8ToAnsi(firstFile);
			DcmFileFormat dcmFile;
			if (dcmFile.loadFile(firstFileAnsi.c_str()).good() && dcmFile.getDataset())
			{
				OFString seriesUID;
				if (dcmFile.getDataset()->findAndGetOFString(DCM_SeriesInstanceUID, seriesUID).good())
				{
					targetUID = seriesUID.c_str();
					targetUID.erase(targetUID.find_last_not_of(" \n\r\t") + 1);
				}
			}
		}

		if (targetUID.empty())
		{
			LogError("Could not read SeriesInstanceUID from target file.");
			return false;
		}

		std::vector<std::string> uids = nameGen->GetSeriesUIDs();
		std::string seriesToLoad = "";

		for (const auto& uid : uids)
		{
			if (uid == targetUID || uid.find(targetUID) != std::string::npos)
			{
				seriesToLoad = uid;
				break;
			}
		}

		typename ReaderType::FileNamesContainer sortedNames;
		if (seriesToLoad.empty())
		{
			LogInfo("Warning: Target UID match not found. Loading default input file names.");
			sortedNames = nameGen->GetInputFileNames();
		}
		else
		{
			sortedNames = nameGen->GetFileNames(seriesToLoad);
		}

		if (sortedNames.empty())
		{
			LogError("No DICOM files found for the selected series.");
			return false;
		}

		typename ReaderType::Pointer reader = ReaderType::New();
		ImageIOType::Pointer dicomIO = ImageIOType::New();

		reader->SetImageIO(dicomIO);
		reader->SetFileNames(sortedNames);
		reader->ForceOrthogonalDirectionOff();

		reader->Update();

		typename ImageType::Pointer image = reader->GetOutput();
		if (!image) return false;

		auto region = image->GetLargestPossibleRegion();
		auto size = region.GetSize();
		auto spacing = image->GetSpacing();

		if (size[0] == 0 || size[1] == 0 || size[2] == 0) return false;

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
			outInfo->bIsSigned = std::numeric_limits<PixelType>::is_signed;
		}

		//std::string logMsg = "Successfully loaded target series. Slices: " + std::to_string(size[2]) + "\n";
		//LogInfo(logMsg.c_str());

		return true;
	}
	catch (const itk::ExceptionObject& e)
	{
		LogError(("ITK Exception: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
	catch (const std::exception& e)
	{
		LogError(("Exception: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}

bool ReadSeriesWithITK( const std::string& firstFile, uint8_t** outBuffer, size_t* outSize,	HDVolumeInfo* outInfo)
{
	using InputPixelType = short;
	using OutputPixelType = float;
	using InputImageType = itk::Image<InputPixelType, 3>;
	using OutputImageType = itk::Image<OutputPixelType, 3>;
	using ReaderType = itk::ImageSeriesReader<InputImageType>;
	using ImageIOType = itk::GDCMImageIO;
	using NamesGeneratorType = itk::GDCMSeriesFileNames;
	using OrientFilterType = itk::OrientImageFilter<InputImageType, InputImageType>;
	using CastFilterType = itk::CastImageFilter<InputImageType, OutputImageType>;

	try
	{
		if (!outBuffer || !outSize || firstFile.empty())
			return false;

		*outBuffer = nullptr;
		*outSize = 0;

		size_t lastSlash = firstFile.find_last_of("/\\");
		std::string folder = (lastSlash != std::string::npos) ? firstFile.substr(0, lastSlash) : firstFile;

		std::string targetUID;
		std::string targetDescription;

		{
			DcmFileFormat dcmFile;
			std::string firstFileAnsi = Utf8ToAnsi(firstFile);

			if (!dcmFile.loadFile(firstFileAnsi.c_str()).good())
				return false;

			DcmDataset* ds = dcmFile.getDataset();
			if (!ds) 
				return false;

			OFString uid, desc;
			if (!ds->findAndGetOFString(DCM_SeriesInstanceUID, uid).good())
				return false;

			ds->findAndGetOFString(DCM_SeriesDescription, desc);

			targetUID = uid.c_str();
			targetDescription = desc.c_str();

			targetUID.erase(targetUID.find_last_not_of(" \n\r\t") + 1);
			targetUID.erase(0, targetUID.find_first_not_of(" \n\r\t"));

			targetDescription.erase(targetDescription.find_last_not_of(" \n\r\t") + 1);
			targetDescription.erase(0, targetDescription.find_first_not_of(" \n\r\t"));
		}

		if (targetUID.empty())
			return false;

		NamesGeneratorType::Pointer nameGen = NamesGeneratorType::New();

		nameGen->SetUseSeriesDetails(true);
		nameGen->AddSeriesRestriction("0008|0021");
		nameGen->AddSeriesRestriction("0008|103e");
		nameGen->AddSeriesRestriction("0020|0011");

		nameGen->SetDirectory(folder);

		std::vector<std::string> combinedUIDs = nameGen->GetSeriesUIDs();
		std::string seriesToLoad;

		for (const auto& combinedId : combinedUIDs)
		{
			ReaderType::FileNamesContainer filesInGroup = nameGen->GetFileNames(combinedId);
			if (filesInGroup.empty())
				continue;

			DcmFileFormat checkFile;
			if (checkFile.loadFile(filesInGroup[0].c_str()).good() && checkFile.getDataset())
			{
				OFString currentUID, currentDesc;
				checkFile.getDataset()->findAndGetOFString(DCM_SeriesInstanceUID, currentUID);
				checkFile.getDataset()->findAndGetOFString(DCM_SeriesDescription, currentDesc);

				std::string strUID = currentUID.c_str();
				std::string strDesc = currentDesc.c_str();

				strUID.erase(strUID.find_last_not_of(" \n\r\t") + 1);
				strUID.erase(0, strUID.find_first_not_of(" \n\r\t"));
				strDesc.erase(strDesc.find_last_not_of(" \n\r\t") + 1);
				strDesc.erase(0, strDesc.find_first_not_of(" \n\r\t"));

				if (strUID == targetUID && strDesc == targetDescription)
				{
					seriesToLoad = combinedId;
					break;
				}
			}
		}

			for (const auto& combinedId : combinedUIDs) 
			{
				if (combinedId.find(targetUID) != std::string::npos) 
				{
					seriesToLoad = combinedId;
					break;
				}
			}

		ReaderType::FileNamesContainer fileNames;
		if (seriesToLoad.empty())
		{
			if (!combinedUIDs.empty())
				fileNames = nameGen->GetFileNames(combinedUIDs[0]);
			else
				fileNames = nameGen->GetInputFileNames();
		}
		else
		{
			fileNames = nameGen->GetFileNames(seriesToLoad);
		}

		if (fileNames.empty())
			return false;

		ReaderType::Pointer reader = ReaderType::New();
		reader->SetImageIO(ImageIOType::New());
		reader->SetFileNames(fileNames);
		reader->ForceOrthogonalDirectionOff();
		reader->Update();

		OrientFilterType::Pointer orienter = OrientFilterType::New();
		orienter->UseImageDirectionOn();
		orienter->SetDesiredCoordinateOrientation(itk::SpatialOrientation::ITK_COORDINATE_ORIENTATION_RAI);
		orienter->SetInput(reader->GetOutput());
		orienter->Update();

		CastFilterType::Pointer caster = CastFilterType::New();
		caster->SetInput(orienter->GetOutput());
		caster->Update();

		OutputImageType::Pointer image = caster->GetOutput();

		if (!image)
			return false;

		auto region = image->GetLargestPossibleRegion();

		auto size = region.GetSize();

		auto spacing = image->GetSpacing();

		if (size[0] == 0 || size[1] == 0 || size[2] == 0)
			return false;

		const size_t voxelCount = static_cast<size_t>(size[0]) * size[1] * size[2];
		const size_t totalBytes = voxelCount *sizeof(OutputPixelType);

		*outBuffer = new uint8_t[totalBytes];

		memcpy(*outBuffer, image->GetBufferPointer(), totalBytes);
		*outSize = totalBytes;

		if (outInfo)
		{
			outInfo->DimX = static_cast<int>(size[0]);
			outInfo->DimY = static_cast<int>(size[1]);
			outInfo->DimZ = static_cast<int>(size[2]);
			outInfo->SpacingX = static_cast<float>(spacing[0]);
			outInfo->SpacingY = static_cast<float>(spacing[1]);
			outInfo->SpacingZ = static_cast<float>(spacing[2]);
			outInfo->BytesPerVoxel = sizeof(OutputPixelType);
			outInfo->bIsSigned = true;
		}

		return true;
	}
	catch (const itk::ExceptionObject&)
	{
		return false;
	}
	catch (...)
	{
		return false;
	}
}

bool ReadMultiFrameWithDCMTK(DcmDataset* ds, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
{
	Uint16 rows = 0, cols = 0, bitsAllocated = 0, pixelRep = 0;
	Uint32 frames = 1;

	ds->findAndGetUint16(DCM_Rows, rows, 0, OFTrue);
	ds->findAndGetUint16(DCM_Columns, cols, 0, OFTrue);
	ds->findAndGetUint16(DCM_BitsAllocated, bitsAllocated, 0, OFTrue);
	ds->findAndGetUint16(DCM_PixelRepresentation, pixelRep, 0, OFTrue);

	if (!FindTagRecursiveUint32(ds, DCM_NumberOfFrames, frames) || frames <= 1)
	{
		DcmTagKey perFrameTag(0x5200, 0x9230);
		DcmElement* element = nullptr;
		if (ds->findAndGetElement(perFrameTag, element).good() && element && element->ident() == EVR_SQ)
			frames = static_cast<Uint32>(reinterpret_cast<DcmSequenceOfItems*>(element)->card());
	}

	if (rows == 0 || cols == 0 || frames == 0)
		return false;

	std::vector<FrameDataMapDCMTK> framesData;
	bool hasPerFrameData = BuildFramesDataDCMTK(ds, framesData) && !framesData.empty();

	double spacingX = 1.0;
	double spacingY = 1.0;
	OFString pixelSpacingStr;

	if (ds->findAndGetOFString(DCM_PixelSpacing, pixelSpacingStr, 0, OFTrue).good() && !pixelSpacingStr.empty())
	{
		double valY = 1.0, valX = 1.0;
		if (sscanf_s(pixelSpacingStr.c_str(), "%lf\\%lf", &valY, &valX) == 2)
		{
			spacingX = valX;
			spacingY = valY;
		}
	}
	else if (hasPerFrameData && framesData[0].find(DCM_PixelSpacing) != framesData[0].end())
	{
		std::string pSpacing = framesData[0][DCM_PixelSpacing];
		double valY = 1.0, valX = 1.0;
		if (sscanf_s(pSpacing.c_str(), "%lf\\%lf", &valY, &valX) == 2)
		{
			spacingX = valX;
			spacingY = valY;
		}
	}

	double spacingZ = 1.0;
	double spacingBetweenSlices = 0.0;
	double sliceThickness = 0.0;

	if (ds->findAndGetFloat64(DCM_SpacingBetweenSlices, spacingBetweenSlices, 0, OFTrue).good() && spacingBetweenSlices > 0.0)
		spacingZ = spacingBetweenSlices;
	else if (ds->findAndGetFloat64(DCM_SliceThickness, sliceThickness, 0, OFTrue).good() && sliceThickness > 0.0)
		spacingZ = sliceThickness;
	else if (hasPerFrameData)
	{
		if (framesData[0].find(DCM_SliceThickness) != framesData[0].end() && !framesData[0][DCM_SliceThickness].empty())
			spacingZ = std::stod(framesData[0][DCM_SliceThickness]);
		else if (frames > 1 && framesData.size() > 1 && framesData[0].find(DCM_ImagePositionPatient) != framesData[0].end() && framesData[1].find(DCM_ImagePositionPatient) != framesData[1].end())
		{
			double x0, y0, z0, x1, y1, z1;
			if (sscanf_s(framesData[0][DCM_ImagePositionPatient].c_str(), "%lf\\%lf\\%lf", &x0, &y0, &z0) == 3 && sscanf_s(framesData[1][DCM_ImagePositionPatient].c_str(), "%lf\\%lf\\%lf", &x1, &y1, &z1) == 3)
			{
				double diff = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + (z1 - z0) * (z1 - z0));
				if (diff > 0.0) spacingZ = diff;
			}
		}
	}

	double slope = 1.0, intercept = 0.0;
	if (!ds->findAndGetFloat64(DCM_RescaleSlope, slope, 0, OFTrue).good())
	{
		if (hasPerFrameData && framesData[0].find(DCM_RescaleSlope) != framesData[0].end() && !framesData[0][DCM_RescaleSlope].empty())
			slope = std::stod(framesData[0][DCM_RescaleSlope]);
	}
	if (!ds->findAndGetFloat64(DCM_RescaleIntercept, intercept, 0, OFTrue).good())
	{
		if (hasPerFrameData && framesData[0].find(DCM_RescaleIntercept) != framesData[0].end() && !framesData[0][DCM_RescaleIntercept].empty())
			intercept = std::stod(framesData[0][DCM_RescaleIntercept]);
	}

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

bool isMultiFrame(DcmDataset* ds)
{
	Uint32 numberOfFrames = 1;

	bool foundFramesTag = FindTagRecursiveUint32(ds, DCM_NumberOfFrames, numberOfFrames);

	if (!foundFramesTag || numberOfFrames <= 1)
	{
		DcmTagKey perFrameTag(0x5200, 0x9230);
		DcmElement* element = nullptr;
		if (ds->findAndGetElement(perFrameTag, element).good() && element && element->ident() == EVR_SQ)
		{
			DcmSequenceOfItems* perSeq = reinterpret_cast<DcmSequenceOfItems*>(element);
			unsigned long card = perSeq->card();
			if (card > 1)
			{
				numberOfFrames = static_cast<Uint32>(card);
				foundFramesTag = true;
				//LogInfo(("Detected Enhanced DICOM frames via PerFrame Sequence: " + std::to_string(numberOfFrames) + "\n").c_str());
			}
		}
	}

	if (!foundFramesTag || numberOfFrames <= 1)
	{
		DcmElement* element = nullptr;
		if (ds->findAndGetElement(DCM_PixelData, element).good() && element)
		{
			E_TransferSyntax xfer = ds->getCurrentXfer();
			DcmXfer xferSyn(xfer);
			if (xferSyn.isEncapsulated())
			{
				DcmPixelData* pixData = reinterpret_cast<DcmPixelData*>(element);
				DcmPixelSequence* pixSeq = nullptr;
				if (pixData->getEncapsulatedRepresentation(xfer, nullptr, pixSeq).good() && pixSeq)
				{
					unsigned long card = pixSeq->card();
					if (card > 1)
					{
						numberOfFrames = static_cast<Uint32>(card - 1);
						//LogInfo(("Detected Compressed Frames via PixelSequence: " + std::to_string(numberOfFrames) + "\n").c_str());
					}
				}
			}
		}
	}

	return numberOfFrames > 1;
}

extern "C" __declspec(dllexport)
bool ReadDicomSeriesToVolume(const char* firstFilePath, uint8_t** outBuffer, size_t* outSize, HDVolumeInfo* outInfo)
{
	try
	{
		if (firstFilePath == nullptr)
			return false;

		std::string firstFile = firstFilePath;
		std::string firstFileAnsi = Utf8ToAnsi(firstFile);

		DcmFileFormat dcmFile;
		OFCondition status = dcmFile.loadFile(firstFileAnsi.c_str());
		if (!status.good())
		{
			LogError("Failed to load DICOM file with DCMTK\n");
			return false;
		}

		DcmDataset* ds = dcmFile.getDataset();
		if (!ds)
			return false;

		if (isMultiFrame(ds))
			return ReadMultiFrameWithDCMTK(ds, outBuffer, outSize, outInfo);
		else
			return ReadSeriesWithITK(firstFile, outBuffer, outSize, outInfo);
	}
	catch (const itk::ExceptionObject& e)
	{
		LogError(("ITK Exception: " + std::string(e.GetDescription()) + "\n").c_str());
		return false;
	}
	catch (const std::exception& e)
	{
		LogError(("Exception: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}


extern "C" __declspec(dllexport)
void FreeVolumeBuffer(uint8_t * buffer)
{
	delete[] buffer;
}