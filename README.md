[![Build status](https://ci.appveyor.com/api/projects/status/s6f0wfcklpcgih99/branch/master?svg=true)](https://ci.appveyor.com/project/peirick/tesseract-ocr-for-windows/branch/master)

# Visual Studio Projects for Tessearct and dependencies.
This repository should help developers to compile tesseract OCR with Visual Studio.
It contains a *build_tesseract.bat* to build the latest tesseract version.
A simple *test_tesseract.bat* is available to show how to run OCR on different image fileformats and generate a pdf.

# Windows下静态编译 Tessearct4.1.1

## [参考博客](https://www.cnblogs.com/njit-77/p/12878348.html)

> 注意：
tesseract的头文件存放在，各个版本的源码里面，全部拷贝出来即可

## Used Libraries
* [Giflib 5.2.1](http://giflib.sourceforge.net/)
* [libtiff 4.1.0](http://simplesystems.org/libtiff/)
* [zlib 1.2.11](http://www.zlib.net/)
* [libpng 1.6.37]( http://www.libpng.org/pub/png/libpng.html)
* [libjpeg 9d](http://ijg.org/)
* [OpenJPEG 2.3.1](http://www.openjpeg.org/)
* [jbig2enc 0.28](https://github.com/agl/jbig2enc)
* [webp master](https://chromium.googlesource.com/webm/libwebp)
* [leptonica master](https://github.com/DanBloomberg/leptonica)
* [tesseract 4.1.1](https://github.com/tesseract-ocr/tesseract)
