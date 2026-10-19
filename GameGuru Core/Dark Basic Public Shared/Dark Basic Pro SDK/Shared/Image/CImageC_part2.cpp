#ifndef WICKEDENGINE // continued from part1
DARKSDK bool GrabImageCore ( int iID, int iX1, int iY1, int iX2, int iY2, int iTextureFlagForGrab )
{
	#ifdef WICKEDENGINE
	// cannot grab from old graphics system - need to new one exclusive to wicked
	#else
	if(iID<1 || iID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return false;
	}
	if(iX1>=iX2 || iY1>=iY2)
	{
		RunTimeError(RUNTIMEERROR_IMAGEAREAILLEGAL);
		return false;
	}

	//#ifdef WICKEDENGINE
	// No grab from backbuffer functionality for now (is this best way to do thumbnails, etc)
	//#else

	#ifdef DX11
	// Size of grab
	GGFORMAT backFormat;
	GGSURFACE_DESC ddsd;
	LPGGSURFACE pBackBuffer = g_pGlob->pCurrentBitmapSurface;
	if(pBackBuffer)
	{
		// get format of backbuffer
		pBackBuffer->GetDesc(&ddsd);
		backFormat = ddsd.Format;
		if(iX2>(int)ddsd.Width || iY2>(int)ddsd.Height)
		{
			RunTimeError(RUNTIMEERROR_IMAGEAREAILLEGAL);
			return false;
		}
	}

	// Image size
	int iImageWidth=iX2-iX1;
	int iImageHeight=iY2-iY1;

	// Get current render target surface
	if(pBackBuffer)
	{
		// check if image already exists of same size and type
		if ( UpdatePtrImage ( iID ) )
		{
			// check against existing
			if(m_imgptr->iWidth != iImageWidth || m_imgptr->iHeight != iImageHeight )
			{
				// existing image and new image are different sizes - so delete any existing image
                RemoveImage( iID );
			}
		}

		// if image format not internal texture format, delete so can be recreated
		if ( m_imgptr )
		{
			GGSURFACE_DESC imgddsd;
			LPGGSURFACE pTextureInterface = NULL;
			m_imgptr->lpTexture->QueryInterface<ID3D11Texture2D>(&pTextureInterface);
			pTextureInterface->GetDesc(&imgddsd);
			SAFE_RELEASE ( pTextureInterface );
			if ( imgddsd.Format != g_DefaultGGFORMAT )
			{
				RemoveImage( iID );
			}
		}

		// create temp image texture to copy backbuffer to
		LPGGSURFACE pTempTexture = NULL;
		GGSURFACE_DESC TempTextureDesc = { iImageWidth, iImageHeight, 1, 1, backFormat, 1, 0, D3D11_USAGE_DEFAULT, 0, 0, 0 };
		m_pD3D->CreateTexture2D( &TempTextureDesc, NULL, &pTempTexture );
		if ( pTempTexture )
		{
			// copy backbuffer to temp texture
			//D3D11_BOX rc = { 0, 0, 0, (LONG)(iImageWidth), (LONG)(iImageHeight), 1 }; 
			D3D11_BOX rc = { iX1, iY1, 0, (LONG)(iX1+iImageWidth), (LONG)(iY1+iImageHeight), 1 }; 
			m_pImmediateContext->CopySubresourceRegion ( pTempTexture, 0, 0, 0, 0, pBackBuffer, 0, &rc );

			// create image
			if ( m_imgptr == NULL ) MakeFormat ( iID, iImageWidth, iImageHeight, g_DefaultGGFORMAT, 0 );
			if ( UpdatePtrImage ( iID ) )
			{
				// get desc of destination texture
				GGSURFACE_DESC srcddsd;
				LPGGSURFACE pTextureInterface = NULL;
				m_imgptr->lpTexture->QueryInterface<ID3D11Texture2D>(&pTextureInterface);
				pTextureInterface->GetDesc(&srcddsd);
				SAFE_RELEASE ( pTextureInterface );

				/* no need for this, can simply delete image and recreate as correct format
				// first convert texture to BGRA if RGBA (from a load) - for internal resource copying
				if ( srcddsd.Format == DXGI_FORMAT_R8G8B8A8_UNORM )
				{
					LPGGSURFACE pDestDDS = NULL;
					LPGGSURFACE pSourceDDS = pTempTexture;
					GGSURFACE_DESC DestDesc = { srcddsd.Width, srcddsd.Height, 1, 1, g_DefaultGGFORMAT, 1, 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, 0 };
					HRESULT hr = m_pD3D->CreateTexture2D( &DestDesc, NULL, (ID3D11Texture2D**)&pDestDDS );
					if ( pDestDDS )
					{
						D3D11_MAPPED_SUBRESOURCE srcMapped;
						HRESULT hr = m_pImmediateContext->Map( pSourceDDS, 0, D3D11_MAP_READ, 0, &srcMapped );
						if ( SUCCEEDED( hr ) )
						{
							D3D11_MAPPED_SUBRESOURCE destMapped;
							HRESULT hr = m_pImmediateContext->Map( pDestDDS, 0, D3D11_MAP_READ, 0, &destMapped );
							if ( SUCCEEDED( hr ) )
							{
								const size_t size = srcddsd.Width*srcddsd.Height*4;
								unsigned char* pSrc = static_cast<unsigned char*>( srcMapped.pData );
								unsigned char* pDest = static_cast<unsigned char*>( destMapped.pData );
								int offsetSrc = 0;
								int offsetDst = 0;
								int rowOffset = srcMapped.RowPitch % srcddsd.Width;
								for (int row = 0; row < srcddsd.Height; ++row)
								{
									for (int col = 0; col < srcddsd.Width; ++col)
									{
										pDest[offsetDst] = pSrc[offsetSrc+2];
										pDest[offsetDst+1] = pSrc[offsetSrc+1];
										pDest[offsetDst+2] = pSrc[offsetSrc];
										pDest[offsetDst+3] = pSrc[offsetSrc+3];
										offsetSrc += 4;
										offsetDst += 4;
									}
									offsetSrc += rowOffset;
								}
								m_pImmediateContext->Unmap( pDestDDS, 0 );
							}
							m_pImmediateContext->Unmap( pSourceDDS, 0 );
						}

						// create new image in converted format
						m_imgptr->lpTexture

						// release temp staged dest
						SAFE_RELEASE ( pDestDDS );
					}
				}
				*/

				// load grabbed surface data into destination texture
				D3D11_BOX rc = { 0, 0, 0, (LONG)(iImageWidth), (LONG)(iImageHeight), 1 }; 
				m_pImmediateContext->CopySubresourceRegion ( m_imgptr->lpTexture, 0, 0, 0, 0, pTempTexture, 0, &rc );

				// get actual dimensions of texture/image
				m_imgptr->fTexUMax=(float)m_imgptr->iWidth/(float)srcddsd.Width;
				m_imgptr->fTexVMax=(float)m_imgptr->iHeight/(float)srcddsd.Height;

				// Ensure smalltextres are handled
				if(m_imgptr->fTexUMax>1.0f) m_imgptr->fTexUMax=1.0f;
				if(m_imgptr->fTexVMax>1.0f) m_imgptr->fTexVMax=1.0f;
			}
			else
			{
				RunTimeError(RUNTIMEERROR_IMAGEERROR);
				return false;
			}

			// free work-newsurface
			SAFE_RELEASE(pTempTexture);
		}
	}
	else
	{
		RunTimeError(RUNTIMEERROR_IMAGEERROR);
		return false;
	}

	// An additional feature is that images by default are stretched to complete fit a texture (no stretching in DX11=performance)
	///if ( iTextureFlagForGrab==0 || iTextureFlagForGrab==2 )
	///{
	///	StretchImage ( iID, GetPowerSquareOfSize(iImageWidth), GetPowerSquareOfSize(iImageHeight) );
	///}
	#else
	// Size of grab
	LPGGSURFACE pBackBuffer = g_pGlob->pCurrentBitmapSurface;
	if(pBackBuffer)
	{
		// get format of backbuffer
		D3DSURFACE_DESC ddsd;
		HRESULT hRes = pBackBuffer->GetDesc(&ddsd);
		GGFORMAT GGFORMAT = ddsd.Format;
		if(iX2>(int)ddsd.Width || iY2>(int)ddsd.Height)
		{
			RunTimeError(RUNTIMEERROR_IMAGEAREAILLEGAL);
			return false;
		}
	}

	// Image size
	int iImageWidth=iX2-iX1;
	int iImageHeight=iY2-iY1;

	// Get current render target surface
	if(pBackBuffer)
	{
		// get format of backbuffer
		D3DSURFACE_DESC ddsd;
		HRESULT hRes = pBackBuffer->GetDesc(&ddsd);
		GGFORMAT GGFORMAT = ddsd.Format;

		// check if image already exists of same size and type
		if ( UpdatePtrImage ( iID ) )
		{
			// check against existing
			if(m_imgptr->iWidth != iImageWidth || m_imgptr->iHeight != iImageHeight)
			{
				// existing image and new image are different sizes - so delete any existing image
                RemoveImage( iID );
			}
		}

		// create temp image texture to copy backbuffer to (same format at first)
		LPGGTEXTURE pTempTexture=NULL;
		hRes = D3DXCreateTexture (m_pD3D,
								  iImageWidth,
								  iImageHeight,
								  D3DX_DEFAULT,
								  0,
								  GGFORMAT,
								  D3DPOOL_MANAGED,
								  &pTempTexture	       );

		if ( pTempTexture )
		{
			// lock surface
			GGLOCKED_RECT d3dlr;
			hRes = pTempTexture->LockRect ( 0, &d3dlr, 0, 0 );
			if ( SUCCEEDED(hRes) )
			{
				// get size of single pixel (16bit, 24bit or 32bit)
				DWORD dwPixelSize = ImageGetBitDepthFromFormat(GGFORMAT)/8;

				// copy from backbuffer lock to texture lock
				RECT rc = { iX1, iY1, iX2, iY2 };
				GGLOCKED_RECT backlock;
				pBackBuffer->LockRect(&backlock, &rc, D3DLOCK_READONLY);
				LPSTR pPtr = (LPSTR)backlock.pBits;
				if ( pPtr==NULL )
				{
					MessageBox ( NULL, "Tried to read a surface which has no read permissions!", "System Memory Bitmap Error", MB_OK );
					RunTimeError(RUNTIMEERROR_IMAGEERROR);
					SAFE_RELEASE(pTempTexture);
					return false;
				}

				// straight copy or stretch copy
				bool bStretchCopy=false;
				D3DSURFACE_DESC imageddsd;
				pTempTexture->GetLevelDesc(0, &imageddsd);
				if(imageddsd.Width<(DWORD)iImageWidth) bStretchCopy=true;
				if(imageddsd.Height<(DWORD)iImageHeight) bStretchCopy=true;
				if(bStretchCopy==true)
				{
					DWORD dwClipWidth = iImageWidth;
					DWORD dwClipHeight = iImageHeight;
					if(imageddsd.Width<dwClipWidth) dwClipWidth=imageddsd.Width;
					if(imageddsd.Height<dwClipHeight) dwClipHeight=imageddsd.Height;
					float fXBit = (float)iImageWidth/(float)dwClipWidth;
					float fYBit = (float)iImageHeight/(float)dwClipHeight;
					LPSTR pImagePtr = (LPSTR)d3dlr.pBits;
					float fY=0.0f;
					for(DWORD y=0; y<dwClipHeight; y++)
					{
						LPSTR pImgPtr = pImagePtr + (y*d3dlr.Pitch);
						LPSTR pPtr = (LPSTR)backlock.pBits + ((int)fY*backlock.Pitch);
						float fX=0.0f;
						for(DWORD x=0; x<dwClipWidth; x++)
						{
							switch(dwPixelSize)
							{
								case 2 : *(WORD*)(pImgPtr) = *(WORD*)(pPtr+((int)fX*dwPixelSize));	break;
								case 4 : *(DWORD*)(pImgPtr) = *(DWORD*)(pPtr+((int)fX*dwPixelSize));	break;
							}
							pImgPtr+=dwPixelSize;
							fX+=fXBit;
						}
						fY+=fYBit;
					}
				}
				else
				{
					LPSTR pImagePtr = (LPSTR)d3dlr.pBits;
					for(int y=0; y<iImageHeight; y++)
					{
						memcpy(pImagePtr, pPtr, iImageWidth*dwPixelSize);
						pImagePtr+=d3dlr.Pitch;
						pPtr+=backlock.Pitch;
					}
				}
				pBackBuffer->UnlockRect();

				// unlock texture
				pTempTexture->UnlockRect(NULL);
			}

			// create image
			if(m_imgptr==NULL) MakeFormat ( iID, iImageWidth, iImageHeight, g_DefaultGGFORMAT, 0 );
			if ( UpdatePtrImage ( iID ) )
			{
				// load grabbed surface data into destination texture
				LPGGSURFACE pNewSurface = NULL;
				pTempTexture->GetSurfaceLevel(0, &pNewSurface);
				LPGGSURFACE pTexSurface = NULL;
				m_imgptr->lpTexture->GetSurfaceLevel(0, &pTexSurface);

				// LEEFIX - 071002 - No srtetching or filtering if DBV1 mode used
				if ( iTextureFlagForGrab==0 )
				{
					// leefix - dx8->dx9 - if texture exact size of image, no scaling required
					D3DSURFACE_DESC ddsdNewTexture;
					HRESULT hRes = pTexSurface->GetDesc(&ddsdNewTexture);
					if ( iImageWidth==(int)ddsdNewTexture.Width && iImageHeight==(int)ddsdNewTexture.Height )
						hRes = D3DXLoadSurfaceFromSurface(pTexSurface, NULL, NULL, pNewSurface, NULL, NULL, D3DX_FILTER_NONE, m_Color);
					else
						hRes = D3DXLoadSurfaceFromSurface(pTexSurface, NULL, NULL, pNewSurface, NULL, NULL, D3DX_DEFAULT, m_Color);
				}
				else
				{
					if ( iTextureFlagForGrab==2 )
						hRes = D3DXLoadSurfaceFromSurface(pTexSurface, NULL, NULL, pNewSurface, NULL, NULL, D3DX_FILTER_NONE, NULL );
					else
					{
						// leeadd - 201008 - u71 - should not have used color key (mode3)
						if ( iTextureFlagForGrab==3 )
							hRes = D3DXLoadSurfaceFromSurface(pTexSurface, NULL, NULL, pNewSurface, NULL, NULL, D3DX_FILTER_NONE, NULL );
						else
							hRes = D3DXLoadSurfaceFromSurface(pTexSurface, NULL, NULL, pNewSurface, NULL, NULL, D3DX_FILTER_NONE, m_Color);
					}
				}

				SAFE_RELEASE(pNewSurface);
				SAFE_RELEASE(pTexSurface);

				// get actual dimensions of texture/image
				D3DSURFACE_DESC imageddsd;
				m_imgptr->lpTexture->GetLevelDesc(0, &imageddsd);
				m_imgptr->fTexUMax=(float)m_imgptr->iWidth/(float)imageddsd.Width;
				m_imgptr->fTexVMax=(float)m_imgptr->iHeight/(float)imageddsd.Height;

				// Ensure smalltextres are handled
				if(m_imgptr->fTexUMax>1.0f) m_imgptr->fTexUMax=1.0f;
				if(m_imgptr->fTexVMax>1.0f) m_imgptr->fTexVMax=1.0f;
			}
			else
			{
				RunTimeError(RUNTIMEERROR_IMAGEERROR);
				SAFE_RELEASE(pTempTexture);
				return false;
			}

			// free work-newsurface
			SAFE_RELEASE(pTempTexture);
		}
	}
	else
	{
		RunTimeError(RUNTIMEERROR_IMAGEERROR);
		return false;
	}

	// An additional feature is that images by default are stretched to complete fit a texture
	if ( iTextureFlagForGrab==0 || iTextureFlagForGrab==2 )
	{
		StretchImage ( iID, GetPowerSquareOfSize(iImageWidth), GetPowerSquareOfSize(iImageHeight) );
	}
	#endif
	//#endif
	#endif

	// Complete
	return true;
}
#endif

DARKSDK bool GrabImageCore ( int iID, int iX1, int iY1, int iX2, int iY2 )
{
	// Stretch image to fit texture by default (0)
	return GrabImageCore ( iID, iX1, iY1, iX2, iY2, 0 );
}

DARKSDK void TransferImage ( int iDestImgID, int iSrcImageID, int iTransferMode, int iMemblockAssistor )
{
	// iTransferMode:
	// 1 = blue channel specifies one of sixteen IDs, each representing a small 4x4 min-texture described in the indexed memblock passed in

	// validate
	if(iDestImgID<1 || iDestImgID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return;
	}
	if(iSrcImageID<1 || iSrcImageID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return;
	}

	#ifdef DX11
	#else
	// get destination details
	D3DSURFACE_DESC ddsd;
	LPGGSURFACE pDstSurface = NULL;
	if ( !UpdatePtrImage ( iDestImgID ) ) return;
	tagImgData* pDestImagePtr = m_imgptr;
	pDestImagePtr->lpTexture->GetSurfaceLevel(0, &pDstSurface);
	pDstSurface->GetDesc(&ddsd);
	DWORD dwDestImgWidth = ddsd.Width;
	DWORD dwDestImgHeight = ddsd.Height;

	// get source details
	LPGGSURFACE pSrcSurface = NULL;
	if ( !UpdatePtrImage ( iSrcImageID ) ) return;
	tagImgData* pSrcImagePtr = m_imgptr;
	pSrcImagePtr->lpTexture->GetSurfaceLevel(0, &pSrcSurface);
	pSrcSurface->GetDesc(&ddsd);
	DWORD dwSrcImgWidth = ddsd.Width;
	DWORD dwSrcImgHeight = ddsd.Height;

	// must be same size 
	if ( dwSrcImgWidth!=dwDestImgWidth || dwSrcImgHeight!=dwDestImgHeight ) return;

	// go through each pixel and apply transfer logic
	GGLOCKED_RECT d3dDstlock;
	RECT rc = { 0, 0, (LONG)dwSrcImgWidth, (LONG)dwSrcImgHeight };
	if(SUCCEEDED(pDstSurface->LockRect ( &d3dDstlock, &rc, 0 ) ) )
	{
		GGLOCKED_RECT d3dSrclock;
		if(SUCCEEDED(pSrcSurface->LockRect ( &d3dSrclock, &rc, 0 ) ) )
		{
			for(DWORD y=0; y<dwSrcImgHeight; y++)
			{
				LPSTR pDst = (LPSTR)d3dDstlock.pBits + (y*d3dDstlock.Pitch);
				LPSTR pSrc = (LPSTR)d3dSrclock.pBits + (y*d3dSrclock.Pitch);
				for(DWORD x=0; x<dwSrcImgWidth; x++)
				{
					if ( iTransferMode==1 )
					{
						// first get the pixel to work on
						DWORD dwPixelValue = *(DWORD*)pSrc;
						float fTexSelectorV = ((dwPixelValue & 0x00000FF))/255.0f;
						float fTexSelectorCol[17];
						fTexSelectorCol[1] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*0)))*16.0f;
						if ( fTexSelectorCol[1] < 0 ) fTexSelectorCol[1] = 0;
						fTexSelectorCol[2] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*1)))*16.0f;
						if ( fTexSelectorCol[2] < 0 ) fTexSelectorCol[2] = 0;
						fTexSelectorCol[3] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*2)))*16.0f;
						if ( fTexSelectorCol[3] < 0 ) fTexSelectorCol[3] = 0;
						fTexSelectorCol[4] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*3)))*16.0f;
						if ( fTexSelectorCol[4] < 0 ) fTexSelectorCol[4] = 0;
						fTexSelectorCol[5] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*4)))*16.0f;
						if ( fTexSelectorCol[5] < 0 ) fTexSelectorCol[5] = 0;
						fTexSelectorCol[6] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*5)))*16.0f;
						if ( fTexSelectorCol[6] < 0 ) fTexSelectorCol[6] = 0;
						fTexSelectorCol[7] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*6)))*16.0f;
						if ( fTexSelectorCol[7] < 0 ) fTexSelectorCol[7] = 0;
						fTexSelectorCol[8] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*7)))*16.0f;
						if ( fTexSelectorCol[8] < 0 ) fTexSelectorCol[8] = 0;
						fTexSelectorCol[9] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*8)))*16.0f;
						if ( fTexSelectorCol[9] < 0 ) fTexSelectorCol[9] = 0;
						fTexSelectorCol[10] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*9)))*16.0f;
						if ( fTexSelectorCol[10] < 0 ) fTexSelectorCol[10] = 0;
						fTexSelectorCol[11] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*10)))*16.0f;
						if ( fTexSelectorCol[11] < 0 ) fTexSelectorCol[11] = 0;
						fTexSelectorCol[12] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*11)))*16.0f;
						if ( fTexSelectorCol[12] < 0 ) fTexSelectorCol[12] = 0;
						fTexSelectorCol[13] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*12)))*16.0f;
						if ( fTexSelectorCol[13] < 0 ) fTexSelectorCol[13] = 0;
						fTexSelectorCol[14] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*13)))*16.0f;
						if ( fTexSelectorCol[14] < 0 ) fTexSelectorCol[14] = 0;
						fTexSelectorCol[15] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*14)))*16.0f;
						if ( fTexSelectorCol[15] < 0 ) fTexSelectorCol[15] = 0;
						fTexSelectorCol[16] = (float)(0.0625f-fabs(fTexSelectorV-(0.0625f*15)))*16.0f;
						if ( fTexSelectorCol[16] < 0 ) fTexSelectorCol[16] = 0;

						// get reference into memblock mini-texture lookup
						int tx = x-(int(x/4)*4);
						int tz = y-(int(y/4)*4);

						// get pointer to memblock
						DWORD* pMemBlockPot = (DWORD*)GetMemblockPtr ( iMemblockAssistor );

						// get weighted contrib from each pot
						DWORD dwTexPartD[17];
						int texpartdr[17];
						int texpartdg[17];
						int texpartdb[17];
						for ( int i = 1; i <= 16; i++ )
						{
							dwTexPartD[i] = *(pMemBlockPot+(i*16)+(tz*4)+tx);
							texpartdr[i] = (int)(((dwTexPartD[i] & 0x00FF0000) >> 16) * fTexSelectorCol[i]);
							texpartdg[i] = (int)(((dwTexPartD[i] & 0x0000FF00) >> 8 ) * fTexSelectorCol[i]);
							texpartdb[i] = (int)(((dwTexPartD[i] & 0x000000FF)      ) * fTexSelectorCol[i]);
						}

						// combine to a single colour
						int diffusemapr = 0;
						int diffusemapg = 0;
						int diffusemapb = 0;
						for ( int i = 1; i <= 16; i++ ) diffusemapr += texpartdr[i];
						for ( int i = 1; i <= 16; i++ ) diffusemapg += texpartdg[i];
						for ( int i = 1; i <= 16; i++ ) diffusemapb += texpartdb[i];
						if ( diffusemapr>255 ) diffusemapr=255;
						if ( diffusemapg>255 ) diffusemapg=255;
						if ( diffusemapb>255 ) diffusemapb=255;

						// write result into destination, preserve the alpha channel of the dest
						DWORD trgb = (diffusemapr<<16)+(diffusemapg<<8)+(diffusemapb);
						DWORD trgba = *(DWORD*)pDst;
						DWORD talpha = trgba & 0xFF000000;
						*(DWORD*)pDst = talpha + trgb;
					}
					pSrc+=4;
					pDst+=4;
				}
			}
			pSrcSurface->UnlockRect();
		}
		pDstSurface->UnlockRect();
	}
	SAFE_RELEASE(pDstSurface);
	SAFE_RELEASE(pSrcSurface);
	#endif
}

DARKSDK void PasteImageCore ( int iID, int iX, int iY, int iFlag )
{
	if(iID<1 || iID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return;
	}
	if ( !UpdatePtrImage ( iID ) )
	{
		#ifdef WICKEDENGINE
		//Silent error now log instead. So we know exactly where the error is.
		if (iMaxPasteImageLogs > 0)
		{
			char pErr[256]; sprintf(pErr, "Error Image Not Found: PasteImageCore(%d,%d,%d,%d) .", iID, iX, iID, iY, iFlag);
			timestampactivity(0, pErr);
			iMaxPasteImageLogs--;
		}
		#else
		RunTimeError(RUNTIMEERROR_IMAGENOTEXIST);
		#endif
		return;
	}

	// use sprite library to paste image(texture) with polys!
	PasteImage( iID, iX, iY, m_imgptr->fTexUMax, m_imgptr->fTexVMax, iFlag );

	return;
}

DARKSDK void StretchImage ( int iID, int Width, int Height )
{
	// returns true if the image exists
	if ( !UpdatePtrImage ( iID ) )
		return;

	// First ensure texture is not already stretched
	#ifdef DX11
	GGSURFACE_DESC ddsd;
	LPGGSURFACE pSurface = NULL;
	m_imgptr->lpTexture->QueryInterface<ID3D11Texture2D>(&pSurface);
	pSurface->GetDesc(&ddsd);
	SAFE_RELEASE(pSurface);
	if ( ddsd.Width==(DWORD)GetPowerSquareOfSize(ddsd.Width)
	&& ddsd.Height==(DWORD)GetPowerSquareOfSize(ddsd.Height)
	&& m_imgptr->fTexUMax==1.0f && m_imgptr->fTexVMax==1.0f)
	{
		// Image is already power of two and fills entire surface
		return;
	}
	#else
	D3DSURFACE_DESC ddsd;
	LPGGSURFACE pSurface = NULL;
	m_imgptr->lpTexture->GetSurfaceLevel(0, &pSurface);
	pSurface->GetDesc(&ddsd);
	SAFE_RELEASE(pSurface);
	if ( ddsd.Width==(DWORD)GetPowerSquareOfSize(ddsd.Width)
	&& ddsd.Height==(DWORD)GetPowerSquareOfSize(ddsd.Height)
	&& m_imgptr->fTexUMax==1.0f && m_imgptr->fTexVMax==1.0f)
	{
		// Image is already power of two and fills entire surface
		return;
	}

	// Record original Size
	int iOrigWidth = m_imgptr->iWidth;
	int iOrigHeight = m_imgptr->iHeight;

	// Create temp store texture
	LPGGTEXTURE pNewTexture = NULL;
	HRESULT hr = D3DXCreateTexture (  m_pD3D,
									  m_imgptr->iWidth,
									  m_imgptr->iHeight,
									  1,
									  D3DX_DEFAULT,
									  g_DefaultGGFORMAT,
									  D3DPOOL_MANAGED,
									  &pNewTexture			);

	// leave if failed
	if ( pNewTexture==NULL )
		return;

	// Copy this image to store
	LPGGSURFACE pNewSurface = NULL;
	pNewTexture->GetSurfaceLevel(0, &pNewSurface);
	LPGGSURFACE pTexSurface = NULL;
	m_imgptr->lpTexture->GetSurfaceLevel(0, &pTexSurface);
	RECT scrRect = { 0, 0, m_imgptr->iWidth, m_imgptr->iHeight };
	hr = D3DXLoadSurfaceFromSurface(pNewSurface, NULL, NULL, pTexSurface, NULL, &scrRect, D3DX_DEFAULT, 0);
	SAFE_RELEASE(pNewSurface);
	SAFE_RELEASE(pTexSurface);

	// Create new Image, deleting the old one
	DeleteImageCore ( iID );
	MakeFormat ( iID, Width, Height, g_DefaultGGFORMAT, 0 );

	// LEEFIX : 140902 : When created a new texture, the m_imgptr can be different so update it
	if ( !UpdatePtrImage ( iID ) )
	{
		SAFE_RELEASE(pNewSurface);
		SAFE_RELEASE(pTexSurface);
		SAFE_RELEASE(pNewTexture);
		return;
	}

	// Copy store to new image
	pNewTexture->GetSurfaceLevel(0, &pNewSurface);
	m_imgptr->lpTexture->GetSurfaceLevel(0, &pTexSurface);
	hr = D3DXLoadSurfaceFromSurface(pTexSurface, NULL, NULL, pNewSurface, NULL, NULL, D3DX_DEFAULT, 0);
	SAFE_RELEASE(pNewSurface);
	SAFE_RELEASE(pTexSurface);

	// Image is still regarded as original size
	m_imgptr->iWidth = iOrigWidth;			// store the width
	m_imgptr->iHeight  = iOrigHeight;		// store the height

	// Texture takes up entire area
	m_imgptr->fTexUMax=1.0f;
	m_imgptr->fTexVMax=1.0f;

	// Free temp store texture
	SAFE_RELEASE(pNewTexture);
	#endif

	// Complete
	return;
}

DARKSDK void PasteImageCore ( int iID, int iX, int iY )
{
	PasteImageCore ( iID, iX, iY, 0 );
}

DARKSDK void DeleteImageCore ( int iID )
{
	if ( !UpdatePtrImage ( iID ) )
		return;

	#ifdef WICKEDENGINE
	//PE: Mark bad textures that have been deleted.
	//PE: Moved to RemoveImage to catch more.
	//lpBadTexture.push_back(m_imgptr->lpTextureView);
	#endif

	// before release, remove the reference from ALL objects
	// leeadd - 220604 - u54 - scans every object that uses this texture address
	ClearObjectsOfTextureRef ( m_imgptr->lpTexture );

	// clear cube details
	m_imgptr->pCubeMapRef = NULL;
	m_imgptr->iCubeMapFace = 0;

    RemoveImage ( iID );
}

//
// Command Functions
//

DARKSDK void LoadImage ( LPSTR szFilename, int iID, int iTextureFlag, int iDivideTextureSize, int iSilentError )
{
	if(LoadImageCore( szFilename, iID, iTextureFlag, false, iDivideTextureSize )==false)
	{
		if ( iSilentError==0 )
		{
			char pCWD[256]; _getcwd ( pCWD, 256 );
			extern char g_pStartingDirectory[260];
			char checkhome[MAX_PATH];
			strcpy(checkhome, g_pStartingDirectory);
			strcat(checkhome, "\\Files");
			char pErr[512];
			if (stricmp(checkhome, pCWD) == NULL)
			{
				//PE: No need to report CWD if already correct root.
				sprintf(pErr, "IMG: %s,%d,%d,%d", szFilename, iID, iTextureFlag, iDivideTextureSize);
			}
			else
				sprintf(pErr, "CWD:%s - IMG %s,%d,%d,%d", pCWD, szFilename, iID, iTextureFlag, iDivideTextureSize);
			//char pErr[256]; sprintf ( pErr, "CWD:%s\nLOAD IMAGE %s,%d,%d,%d", pCWD, szFilename, iID, iTextureFlag, iDivideTextureSize);
#ifndef NOSTEAMORVIDEO
			timestampactivity(0, pErr);
			#endif
		}
	}
}

DARKSDK void LoadImageSize(LPSTR szFilename, int iID, int x, int y)
{
	iResizeLoadImageX = x;
	iResizeLoadImageY = y;
	return LoadImage(szFilename, iID, 0, 8192, 0);
}

DARKSDK void LoadImage ( LPSTR szFilename, int iID, int iKindOfTexture, int iDivideTextureSize )
{
	return LoadImage ( szFilename, iID, iKindOfTexture, iDivideTextureSize, 0 );
}

DARKSDK void LoadImage ( LPSTR szFilename, int iID, int iKindOfTexture )
{
    LoadImage ( szFilename, iID, iKindOfTexture, 0 );
}

DARKSDK void LoadImage ( LPSTR szFilename, int iID )
{
	int iKindOfTexture = 0;
    LoadImage ( szFilename, iID, iKindOfTexture, 0 );
}

DARKSDK void SaveImage ( LPSTR szFilename, int iID )
{
	SaveImageCore ( szFilename, iID );
}

DARKSDK void SaveImage ( LPSTR szFilename, int iID, int iCompressionMode )
{
	SaveImageCore ( szFilename, iID, iCompressionMode );
}

DARKSDK void GrabImage ( int iID, int iX1, int iY1, int iX2, int iY2 )
{
	GrabImageCore ( iID, iX1, iY1, iX2, iY2 );
}

DARKSDK void GrabImage ( int iID, int iX1, int iY1, int iX2, int iY2, int iTextureFlag )
{
	GrabImageCore ( iID, iX1, iY1, iX2, iY2, iTextureFlag );
}

DARKSDK void PasteImage ( int iID, int iX, int iY )
{
	PasteImageCore ( iID, iX, iY, 0 );
}

DARKSDK void PasteImage ( int iID, int iX, int iY, int iFlag )
{
	PasteImageCore ( iID, iX, iY, iFlag );
}

DARKSDK void DeleteImage ( int iID )
{
	if(iID<1 || iID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return;
	}
	if ( !UpdatePtrImage ( iID ) )
	{
		#ifdef WICKEDENGINE
		//Silent error now log instead. So we know exactly where the error is.
		if (iMaxPasteImageLogs > 0)
		{
			char pErr[512]; sprintf(pErr, "Error Image Not Found: DeleteImage(%d) .", iID);
			timestampactivity(0, pErr);
			iMaxPasteImageLogs--;
		}
		#else
		RunTimeError(RUNTIMEERROR_IMAGENOTEXIST);
		#endif
		return;
	}
	DeleteImageCore ( iID );
}

DARKSDK void RotateImage ( int iID, int iAngle )
{
	// Not Implemented in DBPRO V1 RELEASE
	RunTimeError(RUNTIMEERROR_COMMANDNOWOBSOLETE);
}

DARKSDK int GetImageExistEx ( int iID )
{
	if(iID<1 || iID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return 0;
	}

	// returns true if the image exists
	if ( !UpdatePtrImage ( iID ) )
		return 0;

	// return true
	return 1;
}

DARKSDK LPSTR GetImageName ( int iID )
{
	if(iID<1 || iID>MAXIMUMVALUE)
	{
		RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return NULL;
	}

	// returns true if the image exists
	if ( !UpdatePtrImage ( iID ) )
		return NULL;

	// return true
	return m_imgptr->szShortFilename;
}

DARKSDK LPSTR SetImageName(int iID,char *name)
{
	if (!name) return NULL;
	if (iID<1 || iID>MAXIMUMVALUE)
	{
		//RunTimeError(RUNTIMEERROR_IMAGEILLEGALNUMBER);
		return NULL;
	}

	// returns true if the image exists
	if (!UpdatePtrImage(iID))
		return NULL;

	strcpy(m_imgptr->szShortFilename, name);

	// return true
	return m_imgptr->szShortFilename;
}

//
// New Command Functions
//

DARKSDK void SetImageColorKey ( int iR, int iG, int iB )
{
	// set the color key of an image
	m_Color = GGCOLOR_ARGB ( 255, iR, iG, iB );
}

DARKSDK bool FileExist ( LPSTR szFilename )
{
	GGIMAGE_INFO info;
	HRESULT hRes = 0;
	#ifdef DX11
	#else
	if ( g_bImageBlockActive && g_iImageBlockMode==1 )
	{
		DWORD dwFileInMemorySize = 0;
		LPVOID pFileInMemoryData = NULL;
		char pFinalRelPathAndFile[512];
		GetFileInMemory ( szFilename, &pFileInMemoryData, &dwFileInMemorySize, pFinalRelPathAndFile );
		hRes = D3DXGetImageInfoFromFileInMemory( pFileInMemoryData, dwFileInMemorySize, &info );
	}
	else
		hRes = D3DXGetImageInfoFromFile( szFilename, &info );
	#endif

	if ( hRes==GG_OK )
		return true;
	else
		return false;
}

DARKSDK DWORD LoadIcon ( LPSTR pFilename )
{
	char szRealFilename[ MAX_PATH ];
	strcpy_s( szRealFilename, MAX_PATH, pFilename );
	GG_GetRealPath( szRealFilename, 0 );

	// load icon
	HICON hIconHandle = (HICON)LoadImageA ( NULL, szRealFilename, IMAGE_ICON, 48, 48, LR_LOADFROMFILE );

	// complete
	return (DWORD)hIconHandle;
}

DARKSDK void FreeIcon ( DWORD dwIcon )
{
	// free icon handle
    CloseHandle ( (HICON)dwIcon );
}

//
// Data Access Functions
//

DARKSDK void GetImageData( int iID, DWORD* dwWidth, DWORD* dwHeight, DWORD* dwDepth, LPSTR* pData, DWORD* dwDataSize, bool bLockData )
{
	// Read Data
	if(bLockData==true)
	{
		if ( !UpdatePtrImage ( iID ) )
			return;

		if ( m_imgptr->lpTexture==NULL )
			return;

		// data
		*dwWidth = m_imgptr->iWidth;
		*dwHeight = m_imgptr->iHeight;
		*dwDepth = m_imgptr->iDepth;
		DWORD bitdepth = m_imgptr->iDepth/8;

		#ifdef DX11
		// use actual size, not image size
		LPGGSURFACE pTextureInterface = NULL;
		m_imgptr->lpTexture->QueryInterface<ID3D11Texture2D>(&pTextureInterface);
		D3D11_TEXTURE2D_DESC desc;
		pTextureInterface->GetDesc(&desc);
		if(desc.Width<*dwWidth) *dwWidth=desc.Width;
		if(desc.Height<*dwHeight) *dwHeight=desc.Height;
		SAFE_RELEASE ( pTextureInterface );

		// create system memory version
		ID3D11Texture2D* pTempSysMemTexture = NULL;
		D3D11_TEXTURE2D_DESC StagedDesc = { desc.Width, desc.Height, 1, 1, desc.Format, 1, 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0 };
		m_pD3D->CreateTexture2D( &StagedDesc, NULL, &pTempSysMemTexture );
		if ( pTempSysMemTexture )
		{
			// and copy texture image to it
			D3D11_BOX rc = { 0, 0, 0, (LONG)(*dwWidth), (LONG)(*dwHeight), 1 }; 
			m_pImmediateContext->CopySubresourceRegion(pTempSysMemTexture, 0, 0, 0, 0, m_imgptr->lpTexture, 0, &rc);

			// lock for reading staging texture
			GGLOCKED_RECT d3dlock;
			if(SUCCEEDED(m_pImmediateContext->Map(pTempSysMemTexture, 0, D3D11_MAP_READ, 0, &d3dlock)))
			{
				// copy data
				DWORD dwSizeOfBitmapData = (*dwWidth)*(*dwHeight)*bitdepth;
				*pData = new char[dwSizeOfBitmapData];
				*dwDataSize = dwSizeOfBitmapData;

				// copy from surface
				LPSTR pSrc = (LPSTR)d3dlock.pData;
				LPSTR pPtr = *pData;
				DWORD dwDataWidth=*(dwWidth)*bitdepth;
				for(DWORD y=0; y<*dwHeight; y++)
				{
					memcpy(pPtr, pSrc, dwDataWidth);
					pPtr+=dwDataWidth;
					pSrc+=d3dlock.RowPitch;
				}
				m_pImmediateContext->Unmap(pTempSysMemTexture, 0);
			}
			else
			{
				// leefix - 250604 - u54 - place a one in size to indicate could not lock (exists but protected)
				*dwDataSize = 1;
			}

			// free temp system surface
			SAFE_RELEASE(pTempSysMemTexture);
		}
		#else
		// use actual size, not image size
		D3DSURFACE_DESC desc;
		m_imgptr->lpTexture->GetLevelDesc(0,&desc);
		if(desc.Width<*dwWidth) *dwWidth=desc.Width;
		if(desc.Height<*dwHeight) *dwHeight=desc.Height;

		// create system memory version
		LPGGTEXTURE pTempSysMemTexture = NULL;
		D3DXCreateTexture( m_pD3D, desc.Width, desc.Height, 1, 0, desc.Format, D3DPOOL_SYSTEMMEM, &pTempSysMemTexture );

		// and copy texture image to it
		LPGGSURFACE pTempSysMemSurface = NULL;
		LPGGSURFACE pTempVidMemSurface = NULL;
		pTempSysMemTexture->GetSurfaceLevel ( 0, &pTempSysMemSurface );
		m_imgptr->lpTexture->GetSurfaceLevel ( 0, &pTempVidMemSurface );
		//m_pD3D->UpdateSurface ( pTempVidMemSurface, NULL, pTempSysMemSurface, NULL);
		D3DXLoadSurfaceFromSurface ( pTempSysMemSurface, 0, NULL, pTempVidMemSurface, 0, NULL, D3DX_DEFAULT, 0);

		// lock
		GGLOCKED_RECT d3dlock;
		RECT rc = { 0, 0, (LONG)(*dwWidth), (LONG)(*dwHeight) };
		if(SUCCEEDED(pTempSysMemTexture->LockRect ( 0, &d3dlock, &rc, 0 ) ) )
		{
			// create memory
			DWORD dwSizeOfBitmapData = (*dwWidth)*(*dwHeight)*bitdepth;
			*pData = new char[dwSizeOfBitmapData];
			*dwDataSize = dwSizeOfBitmapData;

			// copy from surface
			LPSTR pSrc = (LPSTR)d3dlock.pBits;
			LPSTR pPtr = *pData;
			DWORD dwDataWidth=*(dwWidth)*bitdepth;
			for(DWORD y=0; y<*dwHeight; y++)
			{
				memcpy(pPtr, pSrc, dwDataWidth);
				pPtr+=dwDataWidth;
				pSrc+=d3dlock.Pitch;
			}
			pTempSysMemTexture->UnlockRect(0);
		}
		else
		{
			// leefix - 250604 - u54 - place a one in size to indicate could not lock (exists but protected)
			*dwDataSize = 1;
		}

		// free temp system surface
		SAFE_RELEASE(pTempVidMemSurface);
		SAFE_RELEASE(pTempSysMemSurface);
		SAFE_RELEASE(pTempSysMemTexture);
		#endif
	}
	else
	{
		// free memory
		delete *pData;
	}
}

DARKSDK void SetImageData( int iID, DWORD dwWidth, DWORD dwHeight, DWORD dwDepth, LPSTR pData, DWORD dwDataSize )
{
	if ( UpdatePtrImage ( iID ) )
	{
		if ( m_imgptr->lpTexture==NULL )
			return;

		// Check new specs with existing one
		if(dwWidth==(DWORD)m_imgptr->iWidth && dwHeight==(DWORD)m_imgptr->iHeight && dwDepth==(DWORD)m_imgptr->iDepth)
		{
			// Same size
		}
		else
		{
			// Recreate
			DeleteImageCore ( iID );
			m_imgptr=NULL;
		}
	}

	// new image
	GGFORMAT destImageFormat = GGFMT_A8R8G8B8;
	if(g_bUseRGBAFormat) destImageFormat = DXGIFORMATR8G8B8A8UNORM;

	if(m_imgptr==NULL)
	{
		MakeFormat ( iID, dwWidth, dwHeight, destImageFormat, 0 );
	}

	// may have changed
	if ( !UpdatePtrImage ( iID ) ) return;
	if ( m_imgptr->lpTexture==NULL ) return;

	// write Data
	#ifdef DX11
	LPGGSURFACE pTempTexture = NULL;
	GGSURFACE_DESC TempTextureDesc = { dwWidth, dwHeight, 1, 1, destImageFormat, 1, 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE, 0 };
	m_pD3D->CreateTexture2D( &TempTextureDesc, NULL, &pTempTexture );
	if ( pTempTexture )
	{
		GGLOCKED_RECT d3dlock;
		DWORD bitdepth = m_imgptr->iDepth/8;
		if(SUCCEEDED(m_pImmediateContext->Map(pTempTexture, 0, D3D11_MAP_WRITE, 0, &d3dlock)))
		{
			LPSTR pDest = (LPSTR)d3dlock.pData;
			LPSTR pPtr = pData;
			DWORD dwDataWidth=dwWidth*bitdepth;
			for(DWORD y=0; y<dwHeight; y++)
			{
				memcpy(pDest, pPtr, dwDataWidth);
				pPtr+=dwDataWidth;
				pDest+=d3dlock.RowPitch;
			}
			m_pImmediateContext->Unmap(pTempTexture,0);
		}

		// copy staging texture to shader resource
		m_pImmediateContext->CopyResource ( m_imgptr->lpTexture, pTempTexture );

		// free work resources
		SAFE_RELEASE(pTempTexture);
	}
	#else
	GGLOCKED_RECT d3dlock;
	DWORD bitdepth = m_imgptr->iDepth/8;
	RECT rc = { 0, 0, (LONG)dwWidth, (LONG)dwHeight };
	if(SUCCEEDED(m_imgptr->lpTexture->LockRect ( 0, &d3dlock, &rc, 0 ) ) )
	{
		// copy from surface
		LPSTR pSrc = (LPSTR)d3dlock.pBits;
		LPSTR pPtr = pData;
		DWORD dwDataWidth=dwWidth*bitdepth;
		for(DWORD y=0; y<dwHeight; y++)
		{
			memcpy(pSrc, pPtr, dwDataWidth);
			pPtr+=dwDataWidth;
			pSrc+=d3dlock.Pitch;
		}
		m_imgptr->lpTexture->UnlockRect(0);
	}
	#endif

	// ensure sprites all updated
	UpdateAllSprites();
}

//
// IMAGE BLOCK CODE
//

void OpenImageBlock	( char* szFilename, int iMode )
{
	// cannot open if already open
	if ( g_iImageBlockMode!=-1 )
		return;

	// Reset exclude path
	strcpy ( g_pImageBlockExcludePath, "" );

	// Create image block details
	g_iImageBlockFilename = new char [ strlen ( szFilename ) + 1 ];
	strcpy ( g_iImageBlockFilename, szFilename );

	// Create path to image block
	char current [ 512 ];
	_getcwd ( current, 512 );
	g_iImageBlockRootPath = new char [ strlen ( current ) + 2 ];
	strcpy ( g_iImageBlockRootPath, current );
	if ( g_iImageBlockRootPath [ strlen(g_iImageBlockRootPath)-1 ]!='\\' )
	{
		// add folder divide at end of path string
		int iLen = strlen(g_iImageBlockRootPath);
		g_iImageBlockRootPath [ iLen+0 ] = '\\';
		g_iImageBlockRootPath [ iLen+1 ] = 0;
	}
	
	// Set the imageblock mode (0-write, 1-read)
	g_bImageBlockActive = true;
	g_iImageBlockMode = iMode;

	// U77 - 060211 - does previously written image block exist, if so, we append to it
	bool bPreviousImageBlockExists = false;
	HANDLE hFile = GG_CreateFile ( g_iImageBlockFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile!=INVALID_HANDLE_VALUE )
	{
		bPreviousImageBlockExists = true;
		CloseHandle ( hFile );
	}

	// Load imageblock (for reading or to load last imageblock from previous write)
	if ( g_iImageBlockMode==1 || bPreviousImageBlockExists==true )
	{
		// open to read
		HANDLE hFile = GG_CreateFile ( g_iImageBlockFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		DWORD dwReadBytes = 0;

		// read file list
		int iListMax = 0;
		g_ImageBlockListFile.clear();
		ReadFile ( hFile, &iListMax, sizeof(int), &dwReadBytes, NULL );
		for ( int i = 0; i < iListMax; i++ )
		{
			// write filename length and string
			DWORD dwFilenameLength = 0;
			ReadFile ( hFile, &dwFilenameLength, sizeof(DWORD), &dwReadBytes, NULL );
			LPSTR pFile = new char [ dwFilenameLength ];
			ReadFile ( hFile, pFile, dwFilenameLength, &dwReadBytes, NULL );
			g_ImageBlockListFile.push_back ( pFile );

			// write file offset in data
			DWORD dwListOffset = 0;
			ReadFile ( hFile, &dwListOffset, sizeof(DWORD), &dwReadBytes, NULL );
			g_ImageBlockListOffset.push_back ( dwListOffset );

			// write file size in data
			DWORD dwListSize = 0;
			ReadFile ( hFile, &dwListSize, sizeof(DWORD), &dwReadBytes, NULL );
			g_ImageBlockListSize.push_back ( dwListSize );
		}

		// write the imageblock data itself
		ReadFile ( hFile, &g_dwImageBlockSize, sizeof(DWORD), &dwReadBytes, NULL );
		g_pImageBlockPtr = new char [ g_dwImageBlockSize ];
		ReadFile ( hFile, g_pImageBlockPtr, g_dwImageBlockSize, &dwReadBytes, NULL );
	
		// close file
		CloseHandle ( hFile );
	}
	else
	{
		// Create memory block for saving
		g_dwImageBlockSize = 0;
		g_pImageBlockPtr = NULL;

		// Clear file list
		g_ImageBlockListFile.clear();
		g_ImageBlockListOffset.clear();
		g_ImageBlockListSize.clear();
	}
}

void ExcludeFromImageBlock ( char* szExcludePath )
{
	// exclude any file starting with this string
    if (szExcludePath)
    	strcpy ( g_pImageBlockExcludePath, szExcludePath );
    else
        g_pImageBlockExcludePath[0] = 0;
}

bool AddToImageBlock ( LPSTR pAddFilename )
{
	// can only add in write mode
	if ( g_iImageBlockMode!=0 ) return true;

	// if exist
	if ( !pAddFilename ) return false;

	// exclude if path matches excluder, but only if excluder has a value
    if (g_pImageBlockExcludePath && g_pImageBlockExcludePath[0])
    	if ( strnicmp ( g_pImageBlockExcludePath, pAddFilename, strlen(g_pImageBlockExcludePath) )==NULL )
	    	return true;

	// ensure it does not already exist
	for ( int i = 0; i < (int)g_ImageBlockListFile.size ( ); i++ )
		if ( _stricmp ( g_ImageBlockListFile [ i ], pAddFilename )==NULL )
			return true;

	// open the file
	HANDLE hFile = GG_CreateFile ( pAddFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if ( hFile==INVALID_HANDLE_VALUE ) return false;

	// read the file data
	DWORD dwReadBytes = 0;
	DWORD dwFileSize = GetFileSize ( hFile, NULL );
	LPSTR pFileData = new char [ dwFileSize ];
	ReadFile ( hFile, pFileData, dwFileSize, &dwReadBytes, NULL );
	
	// close file
	CloseHandle ( hFile );

	// create space in the imageblock
	DWORD dwNewSize = g_dwImageBlockSize + dwFileSize;

	// U76 - 020710 - image blocks can get LARGE enough to overrun virtual address space
	// so catch the exeption raised in this case and end the image block creation process gracefully
	LPSTR pNewData = NULL;
	try
	{
		pNewData = new char [ dwNewSize ];
	}
	catch(...)
	{
		// failed to create a new "continuous" block of memory in virtual address space
		// so end image block creation and exit here
		SAFE_DELETE ( pFileData );
		CloseImageBlock();
		return false;
	}
	memcpy ( pNewData, g_pImageBlockPtr, g_dwImageBlockSize );

	// add data to the imageblock
	LPSTR pNewDataInsertPtr = pNewData + g_dwImageBlockSize;
	DWORD dwOffsetToDataInImageBlock = g_dwImageBlockSize;
	memcpy ( pNewDataInsertPtr, pFileData, dwFileSize );

	// free individual file data
	SAFE_DELETE ( pFileData );

	// erase old imageblock and use new one
	SAFE_DELETE ( g_pImageBlockPtr );
	g_dwImageBlockSize = dwNewSize;
	g_pImageBlockPtr = pNewData;

	// add it to list
	LPSTR pListFilename = new char [ strlen ( pAddFilename )+1 ];
	strcpy ( pListFilename, pAddFilename );
	g_ImageBlockListFile.push_back ( pListFilename );
	g_ImageBlockListOffset.push_back ( dwOffsetToDataInImageBlock );
	g_ImageBlockListSize.push_back ( dwFileSize );

	// success
	return true;
}

LPSTR RetrieveFromImageBlock ( LPSTR pRetrieveFilename, DWORD* pdwFileSize )
{
	// find the file
	int iIndexInListFound = -1;
	for ( int iIndexInList = 0; iIndexInList < (int)g_ImageBlockListFile.size ( ); iIndexInList++ )
	{
		if ( _stricmp ( g_ImageBlockListFile [ iIndexInList ], pRetrieveFilename )==NULL )
		{
			iIndexInListFound = iIndexInList;
			break;
		}
	}
	if ( iIndexInListFound==-1 )
	{
		// not found 
		return NULL;
	}

	// locate file within imageblock
	DWORD dwOffset = g_ImageBlockListOffset [ iIndexInListFound ];
	DWORD dwSize = g_ImageBlockListSize [ iIndexInListFound ];

	// return ptr and size
	if ( pdwFileSize ) *pdwFileSize = dwSize;
	return g_pImageBlockPtr + dwOffset;
}

void CloseImageBlock ( void )
{
	// cannot close if already closed
	if ( g_iImageBlockMode==-1 )
		return;

	// Save imageblock
	if ( g_iImageBlockMode==0 )
	{
		// set original path
		char storedir [ 512 ];
		_getcwd ( storedir, 512 );
		_chdir ( g_iImageBlockRootPath );

		// open to write
		HANDLE hFile = GG_CreateFile ( g_iImageBlockFilename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		DWORD dwWrittenBytes = 0;

		// write file list
		int iListMax = g_ImageBlockListFile.size ( );
		WriteFile ( hFile, &iListMax, sizeof(int), &dwWrittenBytes, NULL );
		for ( int i = 0; i < iListMax; i++ )
		{
			// write filename length and string
			LPSTR pListPtr = g_ImageBlockListFile [ i ];
			DWORD dwFilenameLength = strlen(pListPtr)+1;
			WriteFile ( hFile, &dwFilenameLength, sizeof(DWORD), &dwWrittenBytes, NULL );
			WriteFile ( hFile, pListPtr, dwFilenameLength, &dwWrittenBytes, NULL );

			// write file offset in data
			DWORD dwListOffset = g_ImageBlockListOffset [ i ];
			WriteFile ( hFile, &dwListOffset, sizeof(DWORD), &dwWrittenBytes, NULL );

			// write file size in data
			DWORD dwListSize = g_ImageBlockListSize [ i ];
			WriteFile ( hFile, &dwListSize, sizeof(DWORD), &dwWrittenBytes, NULL );
		}

		// write the imageblock data itself
		WriteFile ( hFile, &g_dwImageBlockSize, sizeof(DWORD), &dwWrittenBytes, NULL );
		WriteFile ( hFile, g_pImageBlockPtr, g_dwImageBlockSize, &dwWrittenBytes, NULL );
	
		// close file
		CloseHandle ( hFile );

		// restore folder
		_chdir ( storedir );
	}

	// free strings in imageblock file list
	for ( int i = 0; i < (int)g_ImageBlockListFile.size(); i++ )
		SAFE_DELETE ( g_ImageBlockListFile [ i ] );
	g_ImageBlockListFile.clear();
	g_ImageBlockListOffset.clear();
	g_ImageBlockListSize.clear();

	// free filename
	SAFE_DELETE ( g_iImageBlockFilename );
	SAFE_DELETE ( g_iImageBlockRootPath );

	// Close imageblock
	SAFE_DELETE ( g_pImageBlockPtr );

	// Switch off imageblock
	g_bImageBlockActive = false;
	g_iImageBlockMode = -1;
}

void PerformChecklistForImageBlockFiles ( void )
{
	// Generate Checklist
	DWORD dwMaxStringSizeInEnum=0;
	bool bCreateChecklistNow=false;
	g_pGlob->checklisthasvalues=false;
	g_pGlob->checklisthasstrings=true;
	for(int pass=0; pass<2; pass++)
	{
		if(pass==1)
		{
			// Ensure checklist is large enough
			bCreateChecklistNow=true;
			for(int c=0; c<g_pGlob->checklistqty; c++)
				GlobExpandChecklist(c, dwMaxStringSizeInEnum);
		}

		// Look at parameters
		g_pGlob->checklistqty=0;
		for ( int i = 0; i < (int)g_ImageBlockListFile.size ( ); i++ )
		{
			// write filename length and string
			LPSTR pListPtr = g_ImageBlockListFile [ i ];
			if ( !pListPtr ) continue;

			// Add to checklist
			DWORD dwSize = strlen(pListPtr);
			if(dwSize>dwMaxStringSizeInEnum) dwMaxStringSizeInEnum=dwSize;
			if(bCreateChecklistNow)
			{
				// New checklist item
				strcpy(g_pGlob->checklist[g_pGlob->checklistqty].string, pListPtr);
				g_pGlob->checklist[g_pGlob->checklistqty].valuea = 0;
				g_pGlob->checklist[g_pGlob->checklistqty].valueb = 0;
				g_pGlob->checklist[g_pGlob->checklistqty].valuec = 0;
				g_pGlob->checklist[g_pGlob->checklistqty].valued = 0;
			}
			g_pGlob->checklistqty++;
		}
	}
 
	// Determine if checklist has any contents
	if(g_pGlob->checklistqty>0)
		g_pGlob->checklistexists=true;
	else
		g_pGlob->checklistexists=false;
}

int GetImageFileExist ( LPSTR pFilename )
{
	// If no string, no file
	if ( pFilename==NULL ) return 0;
	char VirtualFilename[_MAX_PATH];
	strcpy(VirtualFilename, pFilename);

	CheckForWorkshopFile ( VirtualFilename );

	// if image block file, quick early out using imageblock
	if ( g_bImageBlockActive && g_iImageBlockMode==1 )
	{
		// final storage string of path and file resolver (makes the filename and path uniform for imageblock retrieval)
		// and work out true file and path, then look for it in imageblock
		char pFinalRelPathAndFile[512];
		LPVOID pFileInMemoryData = 0;
		DWORD dwFileInMemorySize = 0;
		GetFileInMemory ( VirtualFilename, &pFileInMemoryData, &dwFileInMemorySize, pFinalRelPathAndFile );
		if ( pFileInMemoryData )
			return 1;
	}

	// real file
	HANDLE hfile = GG_CreateFile ( VirtualFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hfile==INVALID_HANDLE_VALUE)
		return 0;

	// success, it exists
	CloseHandle(hfile);
	return 1;
}

bool LoadAndSaveUsingDirectXTex ( LPSTR pLoadFile, LPSTR pSaveFile )
{
	HRESULT hRes;

	// load image
	wchar_t wTexLoadFilename[512];
	MultiByteToWideChar(CP_ACP, 0, pLoadFile, -1, wTexLoadFilename, sizeof(wTexLoadFilename));
	DirectX::TexMetadata imageData;
	DirectX::ScratchImage imageTexture;
	if ( strnicmp ( pLoadFile + strlen(pLoadFile) - 4, ".tga", 4 ) == NULL )
		hRes = DirectX::LoadFromTGAFile( wTexLoadFilename, &imageData, imageTexture );
	else
		hRes = DirectX::LoadFromWICFile( wTexLoadFilename, 0, &imageData, imageTexture );

	// save DDS image
	wchar_t wTexSaveFilename[512];
	MultiByteToWideChar(CP_ACP, 0, pSaveFile, -1, wTexSaveFilename, sizeof(wTexSaveFilename));
	const DirectX::Image* img = imageTexture.GetImages();
	hRes = SaveToDDSFile( img, imageTexture.GetImageCount(), imageTexture.GetMetadata(), DirectX::DDS_FLAGS_NONE, wTexSaveFilename );

	// result
	if ( FileExist ( pSaveFile ) == 1 )
		return 1;
	else
		return 0;
}

void ImageCreateCubeMapFile(LPSTR pCubeToSave, LPSTR pUp, LPSTR pDown, LPSTR pLeft, LPSTR pRight, LPSTR pBack, LPSTR pFront)
{
	// collect images
	HRESULT hr;
    size_t images = 0;
    DirectX::TexMetadata info;
    std::vector<std::unique_ptr<DirectX::ScratchImage>> loadedImages;
	for (int cubeside = 0; cubeside < 6; cubeside++ )
	{
		// get filenames as wchars
		wchar_t wOneSideFileName[512];
		if ( cubeside == 0 ) MultiByteToWideChar(CP_ACP, 0, pRight, -1, wOneSideFileName, sizeof(wOneSideFileName));
		if ( cubeside == 1 ) MultiByteToWideChar(CP_ACP, 0, pLeft, -1, wOneSideFileName, sizeof(wOneSideFileName));
		if ( cubeside == 2 ) MultiByteToWideChar(CP_ACP, 0, pUp, -1, wOneSideFileName, sizeof(wOneSideFileName));
		if ( cubeside == 3 ) MultiByteToWideChar(CP_ACP, 0, pDown, -1, wOneSideFileName, sizeof(wOneSideFileName));
		if ( cubeside == 4 ) MultiByteToWideChar(CP_ACP, 0, pFront, -1, wOneSideFileName, sizeof(wOneSideFileName));
		if ( cubeside == 5 ) MultiByteToWideChar(CP_ACP, 0, pBack, -1, wOneSideFileName, sizeof(wOneSideFileName));

		// load the image
	    std::unique_ptr<DirectX::ScratchImage> image(new (std::nothrow)DirectX::ScratchImage);
		if ( strnicmp ( pUp + strlen(pUp)-4, ".dds", 4 ) == NULL )
			hr = DirectX::LoadFromDDSFile( wOneSideFileName, DirectX::DDS_FLAGS_NONE, &info, *image);
		else
			hr = DirectX::LoadFromWICFile( wOneSideFileName, DirectX::DDS_FLAGS_NONE, &info, *image);

		// store in loadedimages list
		images++; loadedImages.push_back(std::move(image));
	}

	// create image array storing the sides
	std::vector<DirectX::Image> imageArray;
	imageArray.reserve(images);
	for (auto it = loadedImages.cbegin(); it != loadedImages.cend(); ++it)
	{
		const DirectX::ScratchImage* simage = it->get();
		assert(simage != 0);
		for (size_t j = 0; j < simage->GetMetadata().arraySize; ++j)
		{
			const DirectX::Image* img = simage->GetImage(0, j, 0);
			assert(img != 0);
			imageArray.push_back(*img);
		}
	}

	// create the cube map from the image array
	DirectX::ScratchImage result;
	hr = result.InitializeCubeFromImages(&imageArray[0], imageArray.size());
	if ( hr == S_OK )
	{
		wchar_t wSaveCubeFile[512];
		MultiByteToWideChar(CP_ACP, 0, pCubeToSave, -1, wSaveCubeFile, sizeof(wSaveCubeFile));
        hr = DirectX::SaveToDDSFile(result.GetImages(), result.GetImageCount(), result.GetMetadata(), DirectX::DDS_FLAGS_NONE, wSaveCubeFile);
	}

	// anything to free
	result.Release();
	for (auto it = loadedImages.cbegin(); it != loadedImages.cend(); ++it)
	{
		DirectX::ScratchImage* simage = it->get();
		simage->Release();
	}
}

void ImageCreateSurfaceTextureChannels(LPSTR pSurfaceToSave, LPSTR pAO, LPSTR pGloss, LPSTR pMetalness, int iO, int iG, int iM, int iA )
{
	// work to largest texture dimension for surface
	int iResultSurfaceWidth = 0;
	int iResultSurfaceHeight = 0;

	// old version of this function didn't work if one texture was missing 
	// e.g. the second texture found would always be placed in gloss/roughness channel even if it was the metalness texture

	// load in contributing textures
	HRESULT hr;
    size_t images = 0;
	bool bLoadedAtLeastOne = false;
    DirectX::TexMetadata info;
	DirectX::ScratchImage* scratchImages[3];
	scratchImages[0] = 0; // AO
	scratchImages[1] = 0; // Roughness
	scratchImages[2] = 0; // Metalness
	for (int contributiontex = 0; contributiontex < 3; contributiontex++ )
	{
		// the filename
		LPSTR pTheFilename = NULL;
		if (contributiontex == 0) pTheFilename = pAO;
		if (contributiontex == 1) pTheFilename = pGloss;
		if (contributiontex == 2) pTheFilename = pMetalness;

		// substitute with stock texture if none supplied
		if (contributiontex == 0 && (pTheFilename == NULL || *pTheFilename == NULL)) pTheFilename = "editors\\gfx\\13.png";

		// can omit AO
		if (pTheFilename && *pTheFilename)
		{
			// get filenames as wchars
			wchar_t wOneSideFileName[512];
			MultiByteToWideChar(CP_ACP, 0, pTheFilename, -1, wOneSideFileName, sizeof(wOneSideFileName));

			// load the image
			DirectX::ScratchImage* image = new DirectX::ScratchImage();
			if (strnicmp(pTheFilename + strlen(pTheFilename) - 4, ".dds", 4) == NULL)
				hr = DirectX::LoadFromDDSFile(wOneSideFileName, DirectX::DDS_FLAGS_NONE, &info, *image);
			else
				hr = DirectX::LoadFromWICFile(wOneSideFileName, DirectX::DDS_FLAGS_NONE, &info, *image);

			// adjust final result surface size
			if (hr == S_OK)
			{
				bLoadedAtLeastOne = true;
				if (iResultSurfaceWidth < info.width) iResultSurfaceWidth = info.width;
				if (iResultSurfaceHeight < info.height) iResultSurfaceHeight = info.height;
			}

			// uncompress so we can use them
			images++;
			if (info.format >= DXGI_FORMAT_BC1_TYPELESS && info.format <= DXGI_FORMAT_BC5_SNORM)
			{
				// store uncompressed in loadedimages list
				scratchImages[ contributiontex ] = new DirectX::ScratchImage();
				hr = DirectX::Decompress(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DXGIFORMATR8G8B8A8UNORM, *scratchImages[contributiontex] );
				image->Release();
				delete image;
			}
			else
			{
				if (info.format != DXGIFORMATR8G8B8A8UNORM)
				{
					// must be in RGBA format, so convert
					scratchImages[ contributiontex ] = new DirectX::ScratchImage();
					hr = DirectX::Convert(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DXGIFORMATR8G8B8A8UNORM, DirectX::TEX_FILTER_DITHER, DirectX::TEX_THRESHOLD_DEFAULT, *scratchImages[contributiontex]);
					image->Release();
					delete image;
				}
				else
				{
					// store in loadedimages list
					scratchImages[contributiontex] = image;
				}
			}
		}
	}

	// if no AO, METALNESS or GLOSS (arg), then assume a small surface with all default sutff in it
	if (bLoadedAtLeastOne == false)
	{
		iResultSurfaceWidth = 32;
		iResultSurfaceHeight = 32;
	}

	// ensure all component images are resized to match surface
    for( int i = 0; i < 3; i++ )
	{
		// resize to match surface dimension
		if ( !scratchImages[i] ) continue;
		DirectX::ScratchImage* resizedImage = new DirectX::ScratchImage();
		hr = DirectX::Resize(scratchImages[i]->GetImages(), scratchImages[i]->GetImageCount(), scratchImages[i]->GetMetadata(), iResultSurfaceWidth, iResultSurfaceHeight, DirectX::TEX_FILTER_DITHER, *resizedImage);
		scratchImages[i]->Release();
		delete scratchImages[i];
		scratchImages[i] = resizedImage;
	}

	// create image for surface
	DirectX::ScratchImage result;
	hr = result.Initialize2D(DXGIFORMATR8G8B8A8UNORM, iResultSurfaceWidth, iResultSurfaceHeight, 1, 1, DirectX::DDS_FLAGS_NONE);

	// copy each texture to a channel in the surface

	uint8_t* pDstPtr = result.GetPixels();
	uint8_t* pAOPtr = scratchImages[0] ? scratchImages[0]->GetPixels() : 0;
	uint8_t* pRoughnessPtr = scratchImages[1] ? scratchImages[1]->GetPixels() : 0;
	uint8_t* pMetalnessPtr = scratchImages[2] ? scratchImages[2]->GetPixels() : 0;
	for (int y = 0; y < iResultSurfaceHeight; y++)
	{
		for (int x = 0; x < iResultSurfaceWidth; x++)
		{
			// ambient occlusion
			if ( pAOPtr ) 
			{
				*(pDstPtr + 0) = *(pAOPtr + iO);
				pAOPtr += 4;
			}
			else *(pDstPtr + 0) = 255; // white if no AO

			// roughness
			if ( pRoughnessPtr ) 
			{
				*(pDstPtr + 1) = *(pRoughnessPtr + iG);
				pRoughnessPtr += 4;
			}
			else *(pDstPtr + 1) = 128; // grey if no roughness

			// metalness
			if ( pMetalnessPtr )
			{
				*(pDstPtr + 2) = *(pMetalnessPtr + iM);
				pMetalnessPtr += 4;
			}
			else *(pDstPtr + 2) = 0; // black if no metalness

			// alpha reflectance
			// iA = in the future we may want to pass in a reflectance map and specify channel where reflectance data is stored
			// for now we will assume reflectance is FULL to allow the per-material reflectance to affect surface globally
			*(pDstPtr + 3) = 255;

			// next pixel please
			pDstPtr += 4;
		}
	}

	// create mipmaps for surface
	DirectX::ScratchImage mipChain;
	hr = DirectX::GenerateMipMaps( result.GetImages(), result.GetImageCount(), result.GetMetadata(), DirectX::TEX_FILTER_SEPARATE_ALPHA, 0, mipChain );

	// compress resulting surface
	DirectX::ScratchImage compressedSurface;
	hr = DirectX::Compress(mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), DXGI_FORMAT_BC1_UNORM, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressedSurface);
	if ( hr == S_OK )
	{
		// ensure destination folder exists (if using imported_models)
		char pPathToSaveTo[MAX_PATH];
		strcpy (pPathToSaveTo, pSurfaceToSave);
		for (int n = strlen(pPathToSaveTo) - 1; n > 0; n--)
		{
			if (pPathToSaveTo[n] == '\\' || pPathToSaveTo[n] == '/')
			{
				pPathToSaveTo[n] = 0;
				break;
			}
		}
		extern int PathExist ( LPSTR pFilename );
		if (PathExist(pPathToSaveTo) == 0)
		{
			// create the imported_models folder
			LPSTR pOnFolderBack = strstr (pPathToSaveTo, "imported_models");
			if ( pOnFolderBack != NULL)
			{
				*pOnFolderBack = 0;
				char pOldDir[MAX_PATH];
				GetCurrentDirectoryA(MAX_PATH, pOldDir);
				SetCurrentDirectoryA(pPathToSaveTo);
				CreateDirectoryA("imported_models", NULL);
				SetCurrentDirectoryA(pOldDir);
			}
		}

		// save the suyrface as a DDS
		wchar_t wSaveSurfaceFile[512];
		MultiByteToWideChar(CP_ACP, 0, pSurfaceToSave, -1, wSaveSurfaceFile, sizeof(wSaveSurfaceFile));
        hr = DirectX::SaveToDDSFile(compressedSurface.GetImages(), compressedSurface.GetImageCount(), compressedSurface.GetMetadata(), DirectX::DDS_FLAGS_NONE, wSaveSurfaceFile);
		compressedSurface.Release();
	}

	// anything to free
	mipChain.Release();
	result.Release();
	for( int i = 0; i < 3; i++)
	{
		if ( scratchImages[i] ) 
		{
			scratchImages[i]->Release();
			delete scratchImages[ i ];
		}
	}
}

// the spawn probe (G-Entity.cpp): named slices of a probed spawn's time, and calls counted in it
void SpawnProbe_Sub ( const char* pName );
long long SpawnProbe_CallStart ( void );
void SpawnProbe_CallEnd ( const char* pName, long long llStart );

// a probed spawn counts the surface maps it makes
void ImageCreateSurfaceTexture(LPSTR pSurfaceToSave, LPSTR pAO, LPSTR pGloss, LPSTR pMetalness )
{
	long long llProbe = SpawnProbe_CallStart();
	ImageCreateSurfaceTextureChannels(pSurfaceToSave, pAO, pGloss, pMetalness, 0, 0, 0, 0);
	SpawnProbe_CallEnd("surface maps made", llProbe);
}

void ImageCreateNormalTextureInvertedGreen(LPSTR pSurfaceToSave, LPSTR pAO, LPSTR pGloss, LPSTR pMetalness, int iO, int iG, int iM, int iA)
{
	// work to largest texture dimension for normal.
	int iResultSurfaceWidth = 0;
	int iResultSurfaceHeight = 0;

	// load in contributing textures
	HRESULT hr;
	size_t images = 0;
	bool bLoadedAtLeastOne = false;
	DirectX::TexMetadata info;
	DirectX::ScratchImage* scratchImages[3];
	scratchImages[0] = 0; 
	scratchImages[1] = 0; 
	scratchImages[2] = 0; 
	for (int contributiontex = 0; contributiontex < 3; contributiontex++)
	{
		// the filename
		LPSTR pTheFilename = NULL;
		if (contributiontex == 0) pTheFilename = pAO;
		if (contributiontex == 1) pTheFilename = pGloss;
		if (contributiontex == 2) pTheFilename = pMetalness;

		// substitute with stock texture if none supplied
		if (contributiontex == 0 && (pTheFilename == NULL || *pTheFilename == NULL)) pTheFilename = "editors\\gfx\\13.png";

		if (pTheFilename && *pTheFilename)
		{
			// get filenames as wchars
			wchar_t wOneSideFileName[512];
			MultiByteToWideChar(CP_ACP, 0, pTheFilename, -1, wOneSideFileName, sizeof(wOneSideFileName));

			// load the image
			DirectX::ScratchImage* image = new DirectX::ScratchImage();
			if (strnicmp(pTheFilename + strlen(pTheFilename) - 4, ".dds", 4) == NULL)
				hr = DirectX::LoadFromDDSFile(wOneSideFileName, DirectX::DDS_FLAGS_NONE, &info, *image);
			else
				hr = DirectX::LoadFromWICFile(wOneSideFileName, DirectX::DDS_FLAGS_NONE, &info, *image);

			// adjust final result size
			if (hr == S_OK)
			{
				bLoadedAtLeastOne = true;
				if (iResultSurfaceWidth < info.width) iResultSurfaceWidth = info.width;
				if (iResultSurfaceHeight < info.height) iResultSurfaceHeight = info.height;
			}

			// uncompress so we can use them
			images++;
			if (info.format >= DXGI_FORMAT_BC1_TYPELESS && info.format <= DXGI_FORMAT_BC5_SNORM)
			{
				// store uncompressed in loadedimages list
				scratchImages[contributiontex] = new DirectX::ScratchImage();
				hr = DirectX::Decompress(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DXGIFORMATR8G8B8A8UNORM, *scratchImages[contributiontex]);
				image->Release();
				delete image;
			}
			else
			{
				if (info.format != DXGIFORMATR8G8B8A8UNORM)
				{
					// must be in RGBA format, so convert
					scratchImages[contributiontex] = new DirectX::ScratchImage();
					hr = DirectX::Convert(image->GetImages(), image->GetImageCount(), image->GetMetadata(), DXGIFORMATR8G8B8A8UNORM, DirectX::TEX_FILTER_DITHER, DirectX::TEX_THRESHOLD_DEFAULT, *scratchImages[contributiontex]);
					image->Release();
					delete image;
				}
				else
				{
					// store in loadedimages list
					scratchImages[contributiontex] = image;
				}
			}
		}
	}

	if (bLoadedAtLeastOne == false)
	{
		iResultSurfaceWidth = 32;
		iResultSurfaceHeight = 32;
	}

	// ensure all component images are resized to match 
	for (int i = 0; i < 3; i++)
	{
		// resize to match dimension
		if (!scratchImages[i]) continue;
		DirectX::ScratchImage* resizedImage = new DirectX::ScratchImage();
		hr = DirectX::Resize(scratchImages[i]->GetImages(), scratchImages[i]->GetImageCount(), scratchImages[i]->GetMetadata(), iResultSurfaceWidth, iResultSurfaceHeight, DirectX::TEX_FILTER_DITHER, *resizedImage);
		scratchImages[i]->Release();
		delete scratchImages[i];
		scratchImages[i] = resizedImage;
	}

	// create image 
	DirectX::ScratchImage result;
	hr = result.Initialize2D(DXGIFORMATR8G8B8A8UNORM, iResultSurfaceWidth, iResultSurfaceHeight, 1, 1, DirectX::DDS_FLAGS_NONE);

	// copy each texture to a channel in the normal map

	uint8_t* pDstPtr = result.GetPixels();
	uint8_t* pRPtr = scratchImages[0] ? scratchImages[0]->GetPixels() : 0;
	uint8_t* pGPtr = scratchImages[1] ? scratchImages[1]->GetPixels() : 0;
	uint8_t* pBPtr = scratchImages[2] ? scratchImages[2]->GetPixels() : 0;
	for (int y = 0; y < iResultSurfaceHeight; y++)
	{
		for (int x = 0; x < iResultSurfaceWidth; x++)
		{
			// Red channel
			if (pRPtr)
			{
				*(pDstPtr + 0) = *(pRPtr + iO);
				pRPtr += 4;
			}
			else *(pDstPtr + 0) = 255; 

			// Green channel
			if (pGPtr)
			{
				// Might not be the best way to invert the green channel. Tiny artefacts appear on the result.
				*(pDstPtr + 1) = 255 - *(pGPtr + iG);
				pGPtr += 4;
			}
			else *(pDstPtr + 1) = 128; 

			// Blue Channel
			if (pBPtr)
			{
				*(pDstPtr + 2) = *(pBPtr + iM);
				pBPtr += 4;
			}
			else *(pDstPtr + 2) = 0; // black if no metalness

			*(pDstPtr + 3) = 255;

			// next pixel please
			pDstPtr += 4;
		}
	}

	// create mipmaps for normal map
	DirectX::ScratchImage mipChain;
	hr = DirectX::GenerateMipMaps(result.GetImages(), result.GetImageCount(), result.GetMetadata(), DirectX::TEX_FILTER_SEPARATE_ALPHA, 0, mipChain);

	// compress result
	DirectX::ScratchImage compressedSurface;
	hr = DirectX::Compress(mipChain.GetImages(), mipChain.GetImageCount(), mipChain.GetMetadata(), DXGI_FORMAT_BC1_UNORM, DirectX::TEX_COMPRESS_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, compressedSurface);
	if (hr == S_OK)
	{
		// ensure destination folder exists (if using imported_models)
		char pPathToSaveTo[MAX_PATH];
		strcpy(pPathToSaveTo, pSurfaceToSave);
		for (int n = strlen(pPathToSaveTo) - 1; n > 0; n--)
		{
			if (pPathToSaveTo[n] == '\\' || pPathToSaveTo[n] == '/')
			{
				pPathToSaveTo[n] = 0;
				break;
			}
		}
		extern int PathExist(LPSTR pFilename);
		if (PathExist(pPathToSaveTo) == 0)
		{
			// create the imported_models folder
			LPSTR pOnFolderBack = strstr(pPathToSaveTo, "imported_models");
			if (pOnFolderBack != NULL)
			{
				*pOnFolderBack = 0;
				char pOldDir[MAX_PATH];
				GetCurrentDirectoryA(MAX_PATH, pOldDir);
				SetCurrentDirectoryA(pPathToSaveTo);
				CreateDirectoryA("imported_models", NULL);
				SetCurrentDirectoryA(pOldDir);
			}
		}
		
		// save the normal map as a DDS
		wchar_t wSaveSurfaceFile[512];
		MultiByteToWideChar(CP_ACP, 0, pSurfaceToSave, -1, wSaveSurfaceFile, sizeof(wSaveSurfaceFile));
		hr = DirectX::SaveToDDSFile(compressedSurface.GetImages(), compressedSurface.GetImageCount(), compressedSurface.GetMetadata(), DirectX::DDS_FLAGS_NONE, wSaveSurfaceFile);
		compressedSurface.Release();
	}

	// anything to free
	mipChain.Release();
	result.Release();
	for (int i = 0; i < 3; i++)
	{
		if (scratchImages[i])
		{
			scratchImages[i]->Release();
			delete scratchImages[i];
		}
	}
}

