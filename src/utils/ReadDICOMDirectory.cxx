#include "ReadDICOMDirectory.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageFileWriter.h"
#include "itkMetaDataObject.h"
// try to use the gdcm scanner instead of the SeriesHelper interface
#include "gdcmSorter.h"
#include "gdcmScanner.h"
#include "gdcmDataSet.h"
#include "gdcmAttribute.h"
#include "gdcmIPPSorter.h"



bool mysort(gdcm::DataSet const & ds1, gdcm::DataSet const & ds2 )
{    
  //gdcm::Attribute<0x0020,0x0013> at1; // Instance Number
  gdcm::Attribute<0x0018,0x1060> at1; // Trigger Time
  gdcm::Attribute<0x0020,0x0032> at11; // Image Position (Patient)
  //fprintf(stdout, "IN SORTER ds1\n");

  at1.Set( ds1 );
  at11.Set( ds1 );
  //gdcm::Attribute<0x0020,0x0013> at2;
  gdcm::Attribute<0x0018,0x1060> at2;
  gdcm::Attribute<0x0020,0x0032> at22;
  at2.Set( ds2 );
  at22.Set( ds2 );
  if( at11 == at22 ) {
    return at1 < at2;
  }
  return at11 < at22;
}


bool mysort_part1(gdcm::DataSet const & ds1, gdcm::DataSet const & ds2 )
{
  gdcm::Attribute<0x0018,0x1060> at1;
  at1.Set( ds1 );
  gdcm::Attribute<0x0018,0x1060> at2;
  at2.Set( ds2 );
  return at1 < at2;
}

bool mysort_part2(gdcm::DataSet const & ds1, gdcm::DataSet const & ds2 ) {
  gdcm::Attribute<0x0020,0x0032> at1;
  at1.Set( ds1 );
  gdcm::Attribute<0x0020,0x0032> at2;
  at2.Set( ds2 );
  return at1 < at2;
}

// technically all files are in the same Frame of Reference, so this function
// should be a no-op
bool mysort_dummy(gdcm::DataSet const & ds1, gdcm::DataSet const & ds2 ) {
  gdcm::Attribute<0x0020,0x0052> at1; // FrameOfReferenceUID
  at1.Set( ds1 );
  gdcm::Attribute<0x0020,0x0052> at2;
  at2.Set( ds2 );
  return at1 < at2;
}

bool Msort(gdcm::DataSet const & ds1, gdcm::DataSet const & ds2 )
{   //     gdcm::Tag(0x20,0x32)
  gdcm::Attribute<0x0020,0x0032> at1;
  at1.Set( ds1 );

  gdcm::Attribute<0x0020,0x0032> at2;  
  at2.Set( ds2 );
  if (at1[2] == at2[2]) {
    return at1[2] < at2[2];
  }
  return at1[2] < at2[2];
}

std::vector<DICOM *> readDICOMDirectory( std::string dirName ) {


  gdcm::Directory dir;
  unsigned int nfiles = dir.Load( dirName );

  // calculate the number of slice positions found (estimates the number of volumes)
  gdcm::Scanner s;
  s.AddTag( gdcm::Tag(0x20,0x32) ); // Image Position (Patient)
  //s.AddTag( gdcm::Tag(0x20,0x37) ); // Image Orientation (Patient)
  s.Scan( dir.GetFilenames() );
  const gdcm::Scanner::ValuesType &values = s.GetValues();
  long nvalues = values.size();
  int nvolumes = std::floor((double)nfiles/nvalues + 0.5f);
  //std::cout << "There are " << nvalues << " different type of values in " << nfiles << " different files." << std::endl;
  //std::cout << "Series is composed of " << nvolumes << " different 3D volumes" << std::endl;


    std::vector<DICOM *> images;

    typedef itk::ImageSeriesReader<DICOMImageType> ReaderType;

    typedef itk::GDCMSeriesFileNames NamesGeneratorType;
    NamesGeneratorType::Pointer nameGenerator = NamesGeneratorType::New();
 
    nameGenerator->SetLoadPrivateTags( true );
    nameGenerator->SetUseSeriesDetails(true);
    nameGenerator->AddSeriesRestriction("0008|0021"); // here add a tag that separates our volumes (like the gradient direction)
    // split the volumes based on the diffusion direction (assumes only a single B0)
    nameGenerator->AddSeriesRestriction("0019|10bb"); // here add a tag that separates our volumes (like the gradient direction)
    nameGenerator->AddSeriesRestriction("0019|10bc"); // here add a tag that separates our volumes (like the gradient direction)
    nameGenerator->AddSeriesRestriction("0019|10bd"); // here add a tag that separates our volumes (like the gradient direction)
    nameGenerator->SetGlobalWarningDisplay(false);
    nameGenerator->SetRecursive( true );
    nameGenerator->SetDirectory(dirName);

    try
    {
        typedef std::vector<std::string> SeriesIdContainer;
        const SeriesIdContainer &seriesUID = nameGenerator->GetSeriesUIDs();
        SeriesIdContainer::const_iterator seriesItr = seriesUID.begin();
        SeriesIdContainer::const_iterator seriesEnd = seriesUID.end();

        if (seriesItr != seriesEnd)
        {
          //std::cout << "The directory: ";
          //std::cout << dirName << std::endl;
          //std::cout << "Contains the following DICOM Series: ";
          //std::cout << std::endl;
        }
        else
        {
          //std::cout << "No DICOMs in: " << dirName << std::endl;
            return images;
        }

        while (seriesItr != seriesEnd)
        {
          //std::cout << seriesItr->c_str() << std::endl;
            ++seriesItr;
        }

        seriesItr = seriesUID.begin();
        while (seriesItr != seriesUID.end())
        {           
            std::string seriesIdentifier;
            seriesIdentifier = seriesItr->c_str();
            seriesItr++;

            typedef std::vector<std::string> FileNamesContainer;
            FileNamesContainer fileNames;
            fileNames = nameGenerator->GetFileNames(seriesIdentifier);

            int volumes = 1;
            std::vector< std::vector<std::string> > detectedVolumes;
            // find out if this should be split again (nvalues could be larger than number of slices in this stack)
            if (fileNames.size() != nvalues) {
                fprintf(stdout, "WARNING: more slices %d found that in stack %d, split did not work correctly\n", (int)fileNames.size(), (int)nvalues);
                // we need to split the fileNames into different volumes
                volumes = (int)std::floor((double)fileNames.size()/nvalues+0.5);
                fprintf(stdout, "WE EXPECT: %d volumes here\n",volumes);
                // split the filenames into different blocks

                gdcm::Scanner s;
                s.AddTag( gdcm::Tag(0x20,0x32) ); // Image Position (Patient)
                s.Scan( fileNames );

                // and sort they by Image Position
                gdcm::Sorter sorter;
                sorter.SetSortFunction( Msort );
                sorter.StableSort( s.GetKeys() );
                
                // now sort each slice position into a different volume
                gdcm::Directory::FilenamesType res = sorter.GetFilenames(); // GetOrderedValues(gdcm::Tag(0x20,0x32));
                gdcm::Directory::FilenamesType::const_iterator file = res.begin();
                detectedVolumes.resize(volumes);
                int count = 0;
                for(; file != res.end(); ++file) {
                   const char *filename = file->c_str();
                   const char *value = s.GetValue(filename, gdcm::Tag(0x20,0x32));
                   int idx = (count % (volumes));
                   fprintf(stdout, "Filename is: %s with value: %s volume %d\n", filename, value, idx);
                   detectedVolumes[idx].push_back(filename);
                   count++;
                }
               /* const gdcm::Scanner::ValuesType &values = s.GetValues();

                for (int i = 0; i < volumes; i++) {
                    std::vector<std::string > tmp(&fileNames[i*nvalues], &fileNames[i*nvalues + nvalues]);
                    fprintf(stdout, "SPLIT INTO %d\n", (int)tmp.size());
                    detectedVolumes.push_back( tmp );
                } */
            } else {
                detectedVolumes.push_back(fileNames);
            }

            // now process the split volumes
            std::vector<std::vector<std::string > >::const_iterator seriesVolumeItr = detectedVolumes.begin();
            int count = 0;
            while (seriesVolumeItr != detectedVolumes.end()) {
                count = count + 1;
                char sname[256];
                sprintf(sname, "%s_%d", seriesIdentifier.c_str(), count);
                std::string seriesIdentifier2 = sname;
                ReaderType::Pointer reader = ReaderType::New();
                typedef itk::GDCMImageIO ImageIOType;
                ImageIOType::Pointer dicomIO = ImageIOType::New();
                dicomIO->SetLoadPrivateTags( true );
                reader->SetImageIO(dicomIO);
                reader->SetFileNames(*seriesVolumeItr);
                reader->Update(); // is required to get the meta data dictionary to fill with values

                // collect the data in the output as well, not just write to file

                /*                typedef itk::ImageFileWriter<DICOMImageType> WriterType;
                WriterType::Pointer writer = WriterType::New();
                std::string outFileName ;
                outFileName = std::string("/tmp/") + seriesIdentifier2 + ".nii";
                writer->SetFileName(outFileName);
                writer->UseCompressionOn(); */
                
                // find out if we have a T1 or DTI images
                DICOMImageType::Pointer im = reader->GetOutput();
                DICOMType type = T1;
                typedef itk::MetaDataDictionary   DictionaryType;
                const DictionaryType & dictionary = dicomIO->GetMetaDataDictionary();
                
                DictionaryType::ConstIterator itr = dictionary.Begin();
                DictionaryType::ConstIterator end = dictionary.End();
                typedef itk::MetaDataObject< std::string > MetaDataStringType;
      
                std::string grad0 = "";
                std::string grad1 = "";
                std::string grad2 = "";
                std::string bval  = "";
                std::string SeriesDescription = "";
                std::string PatientName = "";
                std::string PatientID = "";
                std::string SeriesInstanceUID = "";
                std::string SeriesNumber = "";
                //std::string entryId = "0010|0010";
                //std::string entryId = "0008|103e";


                std::string entryId = "0019|10bb";
                DictionaryType::ConstIterator tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    grad0 = tagvalue;
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0019|10bc";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << std::endl << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    grad1 = tagvalue;
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0019|10bd";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    grad2 = tagvalue;
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0008|103e";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    SeriesDescription = tagvalue;
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0043|1039";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    bval = tagvalue;
                    //fprintf(stdout, "BVAL: %s\n", bval.c_str());
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0010|0010";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    PatientName = tagvalue;
                    //fprintf(stdout, "BVAL: %s\n", bval.c_str());
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0010|0020";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    PatientID = tagvalue;
                    //fprintf(stdout, "BVAL: %s\n", bval.c_str());
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }

                entryId = "0020|000e";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    SeriesInstanceUID = tagvalue;
                    //fprintf(stdout, "BVAL: %s\n", bval.c_str());
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }
                  
                entryId = "0020|0011";
                tagItr = dictionary.Find( entryId );

                if( tagItr == end ) {
                  std::cerr << "Tag " << entryId;
                  std::cerr << " not found in the DICOM header" << std::endl;
                } else {
                  MetaDataStringType::ConstPointer entryvalue =  dynamic_cast<const MetaDataStringType *>( tagItr->second.GetPointer() );
                  if( entryvalue ) {
                    std::string tagvalue = entryvalue->GetMetaDataObjectValue();
                    SeriesNumber = tagvalue;
                    //fprintf(stdout, "BVAL: %s\n", bval.c_str());
                    //std::cout << std::endl << "(" << entryId <<  ") ";
                    //std::cout << " is: " << tagvalue << std::endl;
                  } else {
                    std::cerr << "Entry was not of string type" << std::endl;
                  }
                }
                  
                // we might run into problems later if the image origin is too random (Tolerance is 1.87-e6)
                //itk::Point<double, 3u> origin = im->GetOrigin();
                //fprintf(stdout, "origin is: %g %g %g\n", origin[0], origin[1], origin[2]);
                //im->SetOrigin(origin);
                // we might run into problems later if the image spacing is too random (Tolerance 1.87-e6)

                DICOM *d = new DICOM;
                d->v = im;
                d->grad0 = grad0;
                d->grad1 = grad1;
                d->grad2 = grad2;
                d->bval  = bval;
                d->SeriesDescription = SeriesDescription;
                d->PatientName = PatientName;
                d->PatientID   = PatientID;
                d->SeriesInstanceUID   = SeriesInstanceUID;
                d->SeriesNumber        = SeriesNumber;
                images.push_back(d);

                //outFileName = std::string("/tmp/") + seriesIdentifier2 + "_" + grad0 + "_" + grad1 + "_" + grad2 + ".nii";
                //writer->SetFileName(outFileName);
                //writer->SetInput(images.back()->v);
                //std::cout << std::endl << "Writing: " << outFileName << std::endl;
                //try
                //{
                //    writer->Update();
                //}
                //catch (itk::ExceptionObject &ex)
                //{
                //    std::cout << ex << std::endl;
                //    continue;
                //}
                seriesVolumeItr++;
            }
        }
    }
    catch (itk::ExceptionObject &ex)
    {
        std::cout << ex << std::endl;
        return images;
    }
    return images;
}
