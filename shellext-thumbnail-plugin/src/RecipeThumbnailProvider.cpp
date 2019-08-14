/******************************** Module Header ********************************\
Module Name:  RecipeThumbnailProvider.cpp
Project:      CppShellExtThumbnailHandler
Copyright (c) Microsoft Corporation.

The code sample demonstrates the C++ implementation of a thumbnail handler
for a new file type registered with the .recipe extension.

A thumbnail image handler provides an image to represent the item. It lets you
customize the thumbnail of files with a specific file extension. Windows Vista
and newer operating systems make greater use of file-specific thumbnail images
than earlier versions of Windows. Thumbnails of 32-bit resolution and as large
as 256x256 pixels are often used. File format owners should be prepared to
display their thumbnails at that size.

The example thumbnail handler implements the IInitializeWithStream and
IThumbnailProvider interfaces, and provides thumbnails for .recipe files.
The .recipe file type is simply an XML file registered as a unique file name
extension. It includes an element called "Picture", embedding an image file.
The thumbnail handler extracts the embedded image and asks the Shell to
display it as a thumbnail.

This source is subject to the Microsoft Public License.
See http://www.microsoft.com/opensource/licenses.mspx#Ms-PL.
All other rights reserved.

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
\*******************************************************************************/

#include "RecipeThumbnailProvider.h"
#include <Shlwapi.h>
#include <Wincrypt.h>   // For CryptStringToBinary.
#include <msxml6.h>

#include <windows.h>
#include <algorithm>

#include "../ext-utils.h"
#include "../commands.h"
// depend extension tool file


#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "msxml6.lib")
#pragma comment(lib, "User32.lib")


extern HINSTANCE g_hInst;
extern long g_cDllRef;


RecipeThumbnailProvider::RecipeThumbnailProvider() : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRef);
}


RecipeThumbnailProvider::~RecipeThumbnailProvider()
{
    InterlockedDecrement(&g_cDllRef);
}


#pragma region IUnknown

// Query to the interface the component supported.
IFACEMETHODIMP RecipeThumbnailProvider::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(RecipeThumbnailProvider, IThumbnailProvider),
        QITABENT(RecipeThumbnailProvider, IInitializeWithFile),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

// Increase the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) RecipeThumbnailProvider::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

// Decrease the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) RecipeThumbnailProvider::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef) {
        delete this;
    }

    return cRef;
}

#pragma endregion


#pragma region IInitializeWithFile

// Initializes the thumbnail handler with a file path
IFACEMETHODIMP RecipeThumbnailProvider::Initialize(LPCWSTR pfilePath, DWORD grfMode)
{
    // A handler instance should be initialized only once in its lifetime.
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    filepath_ = seafile::utils::wStringToLocale(pfilePath);
    //if (m_pStream == NULL)
    //{
    //    // Take a reference to the stream if it has not been initialized yet.
    //    hr = pStream->QueryInterface(&m_pStream);
    //}
    // init with a file path

    LOGINFO(L"initialize with file: %s", pfilePath);
    return 1;
}

#pragma endregion


#pragma region IThumbnailProvider

// Gets a thumbnail image and alpha type. The GetThumbnail is called with the
// largest desired size of the image, in pixels. Although the parameter is
// called cx, this is used as the maximum size of both the x and y dimensions.
// If the retrieved thumbnail is not square, then the longer axis is limited
// by cx and the aspect ratio of the original image respected. On exit,
// GetThumbnail provides a handle to the retrieved image. It also provides a
// value that indicates the color format of the image and whether it has
// valid alpha information.
IFACEMETHODIMP RecipeThumbnailProvider::GetThumbnail(UINT cx, HBITMAP *phbmp,
    WTS_ALPHATYPE *pdwAlpha)
{
    LOGINFO(L"get thumbnail");

    // Get diskletter command
    seafile::GetSeadriveMountLetter get_disk_letter_cmd;
    seafile::DISK_LETTER_TYPE seadrive_mount_disk_letter;

    if (!get_disk_letter_cmd.sendAndWait(&seadrive_mount_disk_letter)){

        LOGINFO(L"send get mount disk letter command failed");
        seadrive_mount_disk_letter.clear();
    }

    std::string current_disk_letter = seafile::utils::getDiskLetterName(filepath_);
    transform(current_disk_letter.begin(), current_disk_letter.end(), current_disk_letter.begin(), ::tolower);

    if (seadrive_mount_disk_letter == current_disk_letter) {
        // Get cache status
        seafile::GetCachedStatusCommand cmd(filepath_);
        bool status;
        if (!cmd.sendAndWait(&status)) {
            LOGINFO(L"send get file cached status failed");
        }

        if (status) {
            // TODO: 文件已经缓存到了本地，直接加载已经缓存的文件
            LOGINFO(L"the file have been cached");
        } else {
            // TODO: 文件未进行缓存，请求进行缩略图的请求
            LOGINFO(L"the file have no been cached");
        }

    } else {
        LOGINFO(L"current dir is not in seadrive dir,\
            current dir in diskletter is [%s], seadrive mount diskletter is [%s]",\
            seafile::utils::localeToWString(current_disk_letter),\
            seafile::utils::localeToWString(seadrive_mount_disk_letter)
        );
    }

    *phbmp = (HBITMAP)LoadImage( NULL, L"C:\\Users\\sun\\1.bmp", IMAGE_BITMAP, 0, 0,
               LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );

    if( *phbmp == NULL )
        return FALSE;

    cx = 128;
    return 1;
}

#pragma endregion

// TODO:
#pragma region Helper Functions

HRESULT RecipeThumbnailProvider::ConvertBitmapSourceTo32bppHBITMAP(
    IWICBitmapSource *pBitmapSource, IWICImagingFactory *pImagingFactory,
    HBITMAP *phbmp)
{
    *phbmp = NULL;

    IWICBitmapSource *pBitmapSourceConverted = NULL;
    WICPixelFormatGUID guidPixelFormatSource;
    HRESULT hr = pBitmapSource->GetPixelFormat(&guidPixelFormatSource);

    if (SUCCEEDED(hr) && (guidPixelFormatSource != GUID_WICPixelFormat32bppBGRA)) {
        IWICFormatConverter *pFormatConverter;
        hr = pImagingFactory->CreateFormatConverter(&pFormatConverter);
        if (SUCCEEDED(hr)) {
            // Create the appropriate pixel format converter.
            hr = pFormatConverter->Initialize(pBitmapSource,
                GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL,
                0, WICBitmapPaletteTypeCustom);
            if (SUCCEEDED(hr)) {
                hr = pFormatConverter->QueryInterface(&pBitmapSourceConverted);
            }
            pFormatConverter->Release();
        }
    } else {
        // No conversion is necessary.
        hr = pBitmapSource->QueryInterface(&pBitmapSourceConverted);
    }

    if (SUCCEEDED(hr)) {
        UINT nWidth, nHeight;
        hr = pBitmapSourceConverted->GetSize(&nWidth, &nHeight);
        if (SUCCEEDED(hr)) {
            BITMAPINFO bmi = { sizeof(bmi.bmiHeader) };
            bmi.bmiHeader.biWidth = nWidth;
            bmi.bmiHeader.biHeight = -static_cast<LONG>(nHeight);
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            BYTE *pBits;
            HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS,
                reinterpret_cast<void **>(&pBits), NULL, 0);
            hr = hbmp ? S_OK : E_OUTOFMEMORY;
            if (SUCCEEDED(hr)) {
                WICRect rect = {0, 0, nWidth, nHeight};

                // Convert the pixels and store them in the HBITMAP.
                // Note: the name of the function is a little misleading -
                // we're not doing any extraneous copying here.  CopyPixels
                // is actually converting the image into the given buffer.
                hr = pBitmapSourceConverted->CopyPixels(&rect, nWidth * 4,
                    nWidth * nHeight * 4, pBits);
                if (SUCCEEDED(hr)) {
                    *phbmp = hbmp;
                } else {
                    DeleteObject(hbmp);
                }
            }
        }
        pBitmapSourceConverted->Release();
    }
    return hr;
}


// TODO 如何从文件中读取数据， 然后对缩略图进行渲染
HRESULT RecipeThumbnailProvider::WICCreate32bppHBITMAP(IStream *pstm,
    HBITMAP *phbmp, WTS_ALPHATYPE *pdwAlpha)
{
    *phbmp = NULL;

    // Create the COM imaging factory.
    IWICImagingFactory *pImagingFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pImagingFactory));
    if (SUCCEEDED(hr)) {
        // Create an appropriate decoder.
        IWICBitmapDecoder *pDecoder;
        hr = pImagingFactory->CreateDecoderFromStream(pstm,
            &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, &pDecoder);
        if (SUCCEEDED(hr)) {
            IWICBitmapFrameDecode *pBitmapFrameDecode;
            hr = pDecoder->GetFrame(0, &pBitmapFrameDecode);
            if (SUCCEEDED(hr)) {
                hr = ConvertBitmapSourceTo32bppHBITMAP(pBitmapFrameDecode,
                    pImagingFactory, phbmp);
                if (SUCCEEDED(hr)) {
                    *pdwAlpha = WTSAT_ARGB;
                }
                pBitmapFrameDecode->Release();
            }
            pDecoder->Release();
        }
        pImagingFactory->Release();
    }
    return hr;
}

#pragma endregion
