#ifndef READDICOMDIRECTORY
#define READDICOMDIRECTORY

#include "itkImage.h"
#include <string>
#include <vector>

typedef double DICOMPixelType;
const unsigned int Dimension = 3;
typedef itk::Image<DICOMPixelType, Dimension> DICOMImageType;

enum DICOMType {
    T1,
    DTI
};

// we will get a file back that contains the loaded images with some meta information
struct DICOM {
    DICOMImageType::Pointer v;
    std::string grad0;
    std::string grad1;
    std::string grad2;
    std::string bval;
    std::string SeriesDescription;
    std::string PatientName;
    std::string PatientID;
    std::string SeriesInstanceUID;
    std::string SeriesNumber;
};

// we would like to have the DICOM files turned into Image objects, with the first one being the T1, DTI second
std::vector<DICOM *> readDICOMDirectory( std::string dirName );

#endif
