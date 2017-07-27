### Trivial motion detection in T1 weighted images based on background noise structure

This very simple module makes some assumptions about the orientation of the structural images. You need to know what you are doing. Feel free to adjust the source code to adapt the module to your data.

This program is using an ITK pipeline to read DICOM images and to calculate a measure of the noise structure in one part of the image. You need to have a working ITK installation on your system to compile this program.

Here an example of running the program on a folder with DICOM images (T1 weigthed) provided on the command line:
```
> bin/motion_detection testdata/1.3.12.2.1107.5.2.43.166003.20170208181123.0.0.0
Tag 0019|10bb not found in the DICOM header
Tag 0019|10bc not found in the DICOM header
Tag 0019|10bd not found in the DICOM header
Tag 0043|1039 not found in the DICOM header
{ 
    "motion": "18.5", 
    "dirname": "testdata/1.3.12.2.1107.5.2.43.166003.20170208181123.0.0.0", 
    "PatientID": "NDAR_INVXXXXXXXX", 
    "PatientName": "NDAR_INVXXXXXXXX", 
    "SeriesDescription": "ABCD_T1w_MPR_vNav ", 
    "SeriesInstanceUID": "1.3.12.2.1107.5.2.43.166003.20170208181123.0.0.0", 
    "SeriesNumber": "4 " 
}
```

The returned 'motion' value is unit-less. Scans with more motion would tend to have a larger value.

## How to build

After download and compile of cmake/ITK you can try to build the executable 'motion_detection' with:
```
mkdir bin
cd bin
cmake ../src
make
```

