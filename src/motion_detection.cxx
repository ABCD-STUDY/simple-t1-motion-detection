#include "itkImage.h"
#include <itkHDF5ImageIO.h>
#include "itkImageFileReader.h"
#include "itkNormalizeImageFilter.h"
#include <iostream>
#include <string>
#include "utils/ReadDICOMDirectory.h"
#include "itkCannyEdgeDetectionImageFilter.h"
#include "itkImageFileWriter.h"
#include "itkMaskImageFilter.h"
#include "itkImageRegionIterator.h"

// Motion-related artifacts typically are propagated in the phase encoding direction—in b, along the horizontal axis.
// http://pubs.rsna.org/doi/full/10.1148/rg.313105115

// mask should be in the phase encode direction
void CreateMask(DICOMImageType::Pointer image, DICOMImageType::Pointer &mask) {
  DICOMImageType::RegionType region = image->GetLargestPossibleRegion();
 
  mask->SetRegions(region);
  mask->Allocate();
 
  itk::Point<double, 3u> origin = image->GetOrigin();
  mask->SetOrigin(origin);

  typename DICOMImageType::DirectionType direction = image->GetDirection();
  mask->SetDirection(direction);

  DICOMImageType::SizeType regionSize = region.GetSize();
 
  itk::ImageRegionIterator<DICOMImageType> imageIterator(mask,region);
 
  // Make the left half of the mask white and the right half black
  while(!imageIterator.IsAtEnd()) {
     if(imageIterator.GetIndex()[0] > static_cast<DICOMImageType::IndexValueType>(regionSize[0]) / 3) {
        imageIterator.Set(0);
     } else {
        imageIterator.Set(255);
     }
     ++imageIterator;
  }
}

int main(int argc, char **argv) {
  //std::cout << "Motion detected on input data:" << std::endl;
  if (argc < 2) {
    std::cerr << "Required: <DTI DICOM directory>" << std::endl;
    return EXIT_FAILURE;
  }

  // read the DICOM files as a diffusion series and a T1 series
  std::vector<DICOM* > t1 = readDICOMDirectory( argv[1] );
  
  typedef itk::NormalizeImageFilter< DICOMImageType, DICOMImageType >
    NormalizeFilterType;
  NormalizeFilterType::Pointer normalizeFilter = NormalizeFilterType::New();
  normalizeFilter->SetInput( t1[0]->v );
  typedef itk::StatisticsImageFilter< DICOMImageType > StatisticsFilterType;


  DICOMImageType::RegionType region = t1[0]->v->GetLargestPossibleRegion();
  DICOMImageType::SizeType regionSize = region.GetSize();

  DICOMImageType::RegionType smallInsideRegion;
  DICOMImageType::SizeType smallInsideSize;
  smallInsideSize[0] = regionSize[0]/3; // guess the phase encoding direction
  smallInsideSize[1] = regionSize[1];
  smallInsideSize[2] = regionSize[2];
  smallInsideRegion.SetSize(smallInsideSize);
  smallInsideRegion.SetIndex(region.GetIndex());


  // the normalized image is now in normalizedFilter->GetOutput()
  // do an edge detection on it:
  typedef itk::CannyEdgeDetectionImageFilter <DICOMImageType, DICOMImageType>  CannyEdgeDetectionImageFilterType;
 
  CannyEdgeDetectionImageFilterType::Pointer cannyFilter = CannyEdgeDetectionImageFilterType::New();
  cannyFilter->SetInput(normalizeFilter->GetOutput());
  cannyFilter->SetVariance( 0.1 );
  cannyFilter->SetUpperThreshold( 2 );
  cannyFilter->SetLowerThreshold( 0.06 ); // this seems to be the tricky value
  cannyFilter->GetOutput()->SetRequestedRegion( smallInsideRegion );
    
  cannyFilter->Update(); // forces the processing, would otherwise be done by the file writer

  // write out these images to check what they look like
  /*typedef itk::ImageFileWriter<DICOMImageType> WriterType;
  WriterType::Pointer writer = WriterType::New();
  std::string outFileName ;
  outFileName = std::string("/tmp/test.nii");
  writer->SetFileName(outFileName);
  writer->UseCompressionOn();
  writer->SetInput(cannyFilter->GetOutput());
  writer->Update(); */

  // maybe sufficient if we just create a mask
  /*DICOMImageType::Pointer mask = DICOMImageType::New();
  CreateMask(cannyFilter->GetOutput(), mask);
 
  typedef itk::MaskImageFilter< DICOMImageType, DICOMImageType > MaskFilterType;
  MaskFilterType::Pointer maskFilter = MaskFilterType::New();
  maskFilter->SetInput(cannyFilter->GetOutput());
  maskFilter->SetMaskImage(mask); */

  // write out these images to check what they look like
  /*  writer = WriterType::New();
  outFileName = std::string("/tmp/test_masked.nii");
  writer->SetFileName(outFileName);
  writer->UseCompressionOn();
  writer->SetInput(maskFilter->GetOutput());
  writer->Update(); */
  
  // compute the number of pixel that are white in the phase encode direction
  unsigned long numPixelWhite = 0;
  long totalRegionSize = smallInsideSize[0] * smallInsideSize[1] * smallInsideSize[2];

  itk::ImageRegionIterator<DICOMImageType> imageIterator(cannyFilter->GetOutput(),smallInsideRegion);
 
  while(!imageIterator.IsAtEnd()) {
     double a = imageIterator.Get();
     if (a > 0) {
       numPixelWhite++;
     }
     ++imageIterator;
  }
  fprintf(stdout, "{ \"motion\": \"%.1f\", \"dirname\": \"%s\", \"PatientID\": \"%s\", \"PatientName\": \"%s\", \"SeriesDescription\": \"%s\", \"SeriesInstanceUID\": \"%s\", \"SeriesNumber\": \"%s\" }\n",
          (double)numPixelWhite/totalRegionSize*100.0, argv[1], t1[0]->PatientID.c_str(), t1[0]->PatientName.c_str(),
          t1[0]->SeriesDescription.c_str(), t1[0]->SeriesInstanceUID.c_str(), t1[0]->SeriesNumber.c_str());
  
  /*  writer = WriterType::New();
  outFileName = std::string("/tmp/test_countThese.nii");
  writer->SetFileName(outFileName);
  writer->UseCompressionOn();
  writer->SetInput(cannyFilter->GetOutput());
  writer->Update(); */

  return EXIT_SUCCESS;
}
