/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2011 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "CWindow.h"
#include "../MMU.h"
#include "debug.h"
#include "resource.h"
#include "common.h"
#include <algorithm>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "memView.h"
#include "winutil.h"

using namespace std;

typedef u32 HWAddressType;

enum RegionType {
	MEMVIEW_ARM9 = 0,
	MEMVIEW_ARM7,
	MEMVIEW_FIRMWARE
};

struct MemViewRegion
{
	char name[16];     // name of this region (ex. ARM9, region dropdown)
	char longname[16]; // name of this region (ex. ARM9 memory, window title)
	HWAddressType hardwareAddress; // hardware address of the start of this region
	unsigned int size; // number of bytes to the end of this region
};

const HWAddressType arm9InitAddress = 0x02000000;
const HWAddressType arm7InitAddress = 0x02000000;
static const MemViewRegion s_arm9Region = { "ARM9", "ARM9 memory", arm9InitAddress, 0x1000000 };
static const MemViewRegion s_arm7Region = { "ARM7", "ARM7 memory", arm7InitAddress, 0x1000000 };
static const MemViewRegion s_firmwareRegion = { "Firmware", "Firmware", 0x00000000, 0x40000 };

typedef std::vector<MemViewRegion> MemoryList;
static MemoryList s_memoryRegions;

//////////////////////////////////////////////////////////////////////////////

u8 memRead8 (RegionType regionType, HWAddressType address)
{
	MemViewRegion& region = s_memoryRegions[regionType];
	if (address < region.hardwareAddress || address >= (region.hardwareAddress + region.size))
	{
		return 0;
	}

	u8 value = 0;
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_DumpMemBlock(ARMCPU_ARM9, address, 1, &value);
		return value;
	case MEMVIEW_ARM7:
		MMU_DumpMemBlock(ARMCPU_ARM7, address, 1, &value);
		return value;
	case MEMVIEW_FIRMWARE:
		value = MMU.fw.data[address];
		return value;
	}
	return 0;
}

u16 memRead16 (RegionType regionType, HWAddressType address)
{
	MemViewRegion& region = s_memoryRegions[regionType];
	if (address < region.hardwareAddress || (address + 1) >= (region.hardwareAddress + region.size))
	{
		return 0;
	}

	u16 value = 0;
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_DumpMemBlock(ARMCPU_ARM9, address, 2, (u8*)&value);
		return value;
	case MEMVIEW_ARM7:
		MMU_DumpMemBlock(ARMCPU_ARM7, address, 2, (u8*)&value);
		return value;
	case MEMVIEW_FIRMWARE:
		value = *(u16*)(&MMU.fw.data[address]);
		return value;
	}
	return 0;
}

u32 memRead32 (RegionType regionType, HWAddressType address)
{
	MemViewRegion& region = s_memoryRegions[regionType];
	if (address < region.hardwareAddress || (address + 3) >= (region.hardwareAddress + region.size))
	{
		return 0;
	}

	u32 value = 0;
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_DumpMemBlock(ARMCPU_ARM9, address, 4, (u8*)&value);
		return value;
	case MEMVIEW_ARM7:
		MMU_DumpMemBlock(ARMCPU_ARM7, address, 4, (u8*)&value);
		return value;
	case MEMVIEW_FIRMWARE:
		value = *(u32*)(&MMU.fw.data[address]);
		return value;
	}
	return 0;
}

void memRead(u8* buffer, RegionType regionType, HWAddressType address, size_t size)
{
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_DumpMemBlock(ARMCPU_ARM9, address, size, buffer);
		break;
	case MEMVIEW_ARM7:
		MMU_DumpMemBlock(ARMCPU_ARM7, address, size, buffer);
		break;
	default:
		for (size_t i = 0; i < size; i++)
		{
			buffer[i] = memRead8(regionType, address + i);
		}
		break;
	}
}

bool memIsAvailable(RegionType regionType, HWAddressType address)
{
	if (regionType == MEMVIEW_ARM7 && (address & 0xFFFF0000) == 0x04800000)
	{
		return false;
	}
	else
	{
		return true;
	}
}

void memWrite8 (RegionType regionType, HWAddressType address, u8 value)
{
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_write8(ARMCPU_ARM9, address, value);
		break;
	case MEMVIEW_ARM7:
		MMU_write8(ARMCPU_ARM7, address, value);
		break;
	case MEMVIEW_FIRMWARE:
		MMU.fw.data[address] = value;
		break;
	}
}

void memWrite16 (RegionType regionType, HWAddressType address, u16 value)
{
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_write16(ARMCPU_ARM9, address, value);
		break;
	case MEMVIEW_ARM7:
		MMU_write16(ARMCPU_ARM7, address, value);
		break;
	case MEMVIEW_FIRMWARE:
		*((u16*)&MMU.fw.data[address]) = value;
		break;
	}
}

void memWrite32 (RegionType regionType, HWAddressType address, u32 value)
{
	switch (regionType)
	{
	case MEMVIEW_ARM9:
		MMU_write32(ARMCPU_ARM9, address, value);
		break;
	case MEMVIEW_ARM7:
		MMU_write32(ARMCPU_ARM7, address, value);
		break;
	case MEMVIEW_FIRMWARE:
		*((u32*)&MMU.fw.data[address]) = value;
		break;
	}
}

//////////////////////////////////////////////////////////////////////////////

CMemView::CMemView()
	: CToolWindow(IDD_MEM_VIEW, MemView_DlgProc, "ARM9 memory")
	, region(MEMVIEW_ARM9)
	, viewMode(0)
	, sel(FALSE)
	, selPart(0)
	, selAddress(0x00000000)
	, selNewVal(0x00000000)
{
	// initialize memory regions
	if (s_memoryRegions.empty())
	{
		s_memoryRegions.push_back(s_arm9Region);
		s_memoryRegions.push_back(s_arm7Region);
		s_memoryRegions.push_back(s_firmwareRegion);
	}

	PostInitialize();
}

CMemView::~CMemView()
{
	DestroyWindow(hWnd);
	hWnd = NULL;

	UnregWndClass("MemView_ViewBox");
}

//////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK MemView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CMemView* wnd = (CMemView*)GetWindowLongPtr(hDlg, DWLP_USER);
	if((wnd == NULL) && (uMsg != WM_INITDIALOG))
		return 0;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			HWND cur;

			wnd = (CMemView*)lParam;
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG)wnd);
			SetWindowLongPtr(GetDlgItem(hDlg, IDC_MEMVIEWBOX), DWLP_USER, (LONG)wnd);

			wnd->font = CreateFont(16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, GetFontQuality(), FIXED_PITCH, "Courier New");

			s_memoryRegions[MEMVIEW_ARM9].hardwareAddress = arm9InitAddress;
			s_memoryRegions[MEMVIEW_ARM7].hardwareAddress = arm7InitAddress;
			wnd->address = s_memoryRegions.front().hardwareAddress;

			MemViewRegion& region = s_memoryRegions[wnd->region];

			cur = GetDlgItem(hDlg, IDC_REGION);
			for(MemoryList::iterator iter = s_memoryRegions.begin(); iter != s_memoryRegions.end(); ++iter)
			{
				SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)iter->name);
			}
			SendMessage(cur, CB_SETCURSEL, 0, 0);

			cur = GetDlgItem(hDlg, IDC_VIEWMODE);
			SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)"Bytes");
			SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)"Halfwords");
			SendMessage(cur, CB_ADDSTRING, 0, (LPARAM)"Words");
			SendMessage(cur, CB_SETCURSEL, 0, 0);
			cur = GetDlgItem(hDlg, IDC_ADDRESS);
			SendMessage(cur, EM_SETLIMITTEXT, 8, 0);
			char addressText[9];
			wsprintf(addressText, "%08X", wnd->address);
			SetWindowText(cur, addressText);

			wnd->Refresh();
		}
		return 1;

	case WM_CLOSE:
		CloseToolWindow(wnd);
		return 1;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			CloseToolWindow(wnd);
			return 1;

		case IDC_REGION:
			if ((HIWORD(wParam) == CBN_SELCHANGE) || (HIWORD(wParam) == CBN_CLOSEUP))
			{
				wnd->region = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

				MemViewRegion& region = s_memoryRegions[wnd->region];
				wnd->address = region.hardwareAddress;
				SetScrollRange(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, 0x00000000, (region.size - 1) >> 4, TRUE);
				SetScrollPos(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, 0x00000000, TRUE);

				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

				wnd->SetTitle(region.longname);

				wnd->Refresh();
			}
			return 1;

		case IDC_VIEWMODE:
			if ((HIWORD(wParam) == CBN_SELCHANGE) || (HIWORD(wParam) == CBN_CLOSEUP))
			{
				wnd->viewMode = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

				wnd->Refresh();
			}
			return 1;

		case IDC_GO:
			{
				char addrstr[9];
				int len;
				int i;
				int shift;
				BOOL error = FALSE;
				u32 address = 0x00000000;

				len = GetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), addrstr, 9);

				for(i = 0; i < len; i++)
				{
					char ch = addrstr[i];

					if((ch >= '0') && (ch <= '9'))
						continue;

					if((ch >= 'A') && (ch <= 'F'))
						continue;

					if((ch >= 'a') && (ch <= 'f'))
						continue;

					if(ch == '\0')
						break;

					error = TRUE;
					break;
				}

				if(error)
				{
					MessageBox(hDlg, "Error:\nInvalid address specified.\nThe address must be an hexadecimal value.", "DeSmuME", (MB_OK | MB_ICONERROR));
					SetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), "");
					return 1;
				}

				for(i = (len-1), shift = 0; i >= 0; i--, shift += 4)
				{
					char ch = addrstr[i];

					if((ch >= '0') && (ch <= '9'))
						address |= ((ch - '0') << shift);
					else if((ch >= 'A') && (ch <= 'F'))
						address |= ((ch - 'A' + 0xA) << shift);
					else if((ch >= 'a') && (ch <= 'f'))
						address |= ((ch - 'a' + 0xA) << shift);
				}

				MemViewRegion& region = s_memoryRegions[wnd->region];
				if (wnd->region == MEMVIEW_ARM9 || wnd->region == MEMVIEW_ARM7)
				{
					region.hardwareAddress = address & 0xFF000000;
				}
				HWAddressType addrMin = (region.hardwareAddress) & 0xFFFFFF00;
				HWAddressType addrMax = max(addrMin, (region.hardwareAddress + region.size - 0x100 - 1) & 0xFFFFFF00);

				wnd->address = max((u32)addrMin, min((u32)addrMax, (address & 0xFFFFFFF0)));

				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

				SetScrollPos(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, (((wnd->address - region.hardwareAddress) >> 4) & 0x000FFFFF), TRUE);
				wnd->Refresh();
			}
			return 1;

		case IDC_TEXTDUMP:
			{
				char fileName[256] = "";
				OPENFILENAME ofn;

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0Any file (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = 256;
				ofn.lpstrDefExt = "txt";
				ofn.Flags = OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

				if(GetSaveFileName(&ofn))
				{
					FILE *f;
					u8 memory[0x100];
					int line;

					memRead(memory, (RegionType)wnd->region, wnd->address, 0x100);

					f = fopen(fileName, "a");

					for(line = 0; line < 16; line++)
					{
						int i;
						
						fprintf(f, "%08X\t\t", (wnd->address + (line << 4)));

						switch(wnd->viewMode)
						{
						case 0:
							{
								for(i = 0; i < 16; i++)
								{
									fprintf(f, "%02X ", T1ReadByte(memory, ((line << 4) + i)));
								}
								fprintf(f, "\t");
							}
							break;

						case 1:
							{
								for(i = 0; i < 16; i += 2)
								{
									fprintf(f, "%04X ", T1ReadWord(memory, ((line << 4) + i)));
								}
								fprintf(f, "\t\t");
							}
							break;

						case 2:
							{
								for(i = 0; i < 16; i += 4)
								{
									fprintf(f, "%08X ", T1ReadLong(memory, ((line << 4) + i)));
								}
								fprintf(f, "\t\t\t");
							}
							break;
						}

						for(i = 0; i < 16; i++)
						{
							u8 val = T1ReadByte(memory, ((line << 4) + i));
									
							if((val >= 32) && (val <= 127))
								fprintf(f, "%c", (char)val);
							else
								fprintf(f, ".");
						}
						fprintf(f, "\n");
					}

					fclose(f);
				}
			}
			return 1;

		case IDC_DUMPALL:
		case IDC_RAWDUMP:
			{
				char fileName[256] = "";
				OPENFILENAME ofn;

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Binary file (*.bin)\0*.bin\0Any file (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = 256;
				ofn.lpstrDefExt = "bin";
				ofn.Flags = OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_PATHMUSTEXIST;

				if(GetSaveFileName(&ofn))
				{

					if(LOWORD(wParam) == IDC_RAWDUMP)
					{
						EMUFILE_FILE f(fileName,"ab");
						u8 memory[0x100];
						memRead(memory, (RegionType)wnd->region, wnd->address, 0x100);
						f.fwrite(memory, 0x100);
					}
					else
					{
						EMUFILE_FILE f(fileName,"wb");
						switch(wnd->region)
						{
						case MEMVIEW_ARM9:
						case MEMVIEW_ARM7:
							DEBUG_dumpMemory(&f);
							break;
						default:
							{
								const size_t blocksize = 0x100;
								byte* memory = new byte[blocksize];
								if (memory != NULL)
								{
									MemViewRegion& region = s_memoryRegions[wnd->region];
									for (HWAddressType address = region.hardwareAddress;
										address < region.hardwareAddress + region.size; address += blocksize)
									{
										size_t size = blocksize;
										if (address + size > region.hardwareAddress + region.size)
										{
											size = region.size - (address - region.hardwareAddress);
										}
										memRead(memory, (RegionType)wnd->region, address, size);
										f.fwrite(memory, size);
									}
									delete [] memory;
								}
							}	break;
						}
					}
				}
			}
			return 1;

		case IDC_BIG_ENDIAN:
			wnd->Refresh();
			return 1;
		}
		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT MemView_ViewBoxPaint(CMemView* wnd, HWND hCtl, WPARAM wParam, LPARAM lParam)
{
	HDC				hdc;
	PAINTSTRUCT		ps;
	RECT			rc;
	int				w, h;
	SIZE			fontsize;
	HDC				mem_hdc;
	HBITMAP			mem_bmp;
	u32 			addr = wnd->address;
	u8 				memory[0x100];
	char 			text[80];
	int 			startx;
	int 			curx, cury;
	int 			line;

	GetClientRect(hCtl, &rc);
	w = (rc.right - rc.left);
	h = (rc.bottom - rc.top);

	hdc = BeginPaint(hCtl, &ps);

	mem_hdc = CreateCompatibleDC(hdc);
	mem_bmp = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(mem_hdc, mem_bmp);

	SelectObject(mem_hdc, wnd->font);

	SetBkMode(mem_hdc, OPAQUE);
	SetBkColor(mem_hdc, RGB(255, 255, 255));
	SetTextColor(mem_hdc, RGB(0, 0, 0));

	FillRect(mem_hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	GetTextExtentPoint32(mem_hdc, " ", 1, &fontsize);
	startx = ((fontsize.cx * 8) + 5);
	curx = 0;
	cury = (fontsize.cy + 3);

	MoveToEx(mem_hdc, ((fontsize.cx * 8) + 2), 0, NULL);
	LineTo(mem_hdc, ((fontsize.cx * 8) + 2), h);

	MoveToEx(mem_hdc, 0, (fontsize.cy + 1), NULL);
	LineTo(mem_hdc, w, (fontsize.cy + 1));

	switch(wnd->viewMode)
	{
	case 0: sprintf(text, "   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   0123456789ABCDEF"); break;
	case 1: sprintf(text, "       0 1  2 3  4 5  6 7  8 9  A B  C D  E F       0123456789ABCDEF"); break;
	case 2: sprintf(text, "         0 1 2 3  4 5 6 7  8 9 A B  C D E F         0123456789ABCDEF"); break;
	}
	TextOut(mem_hdc, startx, 0, text, strlen(text));
	
	memRead(memory, (RegionType)wnd->region, wnd->address, 0x100);

	for(line = 0; line < 16; line++, addr += 0x10)
	{
		int i;

		sprintf(text, "%08X", addr);
		TextOut(mem_hdc, 0, cury, text, strlen(text));

		curx = startx;

		switch(wnd->viewMode)
		{
		case 0:
			{
				SetDlgItemText(wnd->hWnd,IDC_2012,"");

				curx += (fontsize.cx * 2);
				for(i = 0; i < 16; i++)
				{
					u8 val = T1ReadByte(memory, ((line << 4) + i));
					if(wnd->sel && (wnd->selAddress == (addr + i)))
					{
						SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						
						switch(wnd->selPart)
						{
						case 0: sprintf(text, "%02X", val); break;
						case 1: sprintf(text, "%01X.", wnd->selNewVal); break;
						}
					}
					else
					{
						SetBkColor(mem_hdc, RGB(255, 255, 255));
						SetTextColor(mem_hdc, RGB(0, 0, 0));
						
						sprintf(text, "%02X", val);
					}

					TextOut(mem_hdc, curx, cury, text, strlen(text));
					curx += (fontsize.cx * (2+1));
				}
				curx += (fontsize.cx * 2);
			}
			break;

		case 1:
			{
				SetDlgItemText(wnd->hWnd,IDC_2012,"");

				curx += (fontsize.cx * 6);
				for(i = 0; i < 16; i += 2)
				{
					u16 val = T1ReadWord(memory, ((line << 4) + i));
					if(IsDlgCheckboxChecked(wnd->hWnd,IDC_BIG_ENDIAN))
					{
						char swp[2];
						swp[0] = (val>>8)&0xFF;
						swp[1] = val&0xFF;
						val = *(u16*)swp;
					}
					if(wnd->sel && (wnd->selAddress == (addr + i)))
					{
						SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						
						switch(wnd->selPart)
						{
						case 0: sprintf(text, "%04X", val); break;
						case 1: sprintf(text, "%01X...", wnd->selNewVal); break;
						case 2: sprintf(text, "%02X..", wnd->selNewVal); break;
						case 3: sprintf(text, "%03X.", wnd->selNewVal); break;
						}
					}
					else
					{
						SetBkColor(mem_hdc, RGB(255, 255, 255));
						SetTextColor(mem_hdc, RGB(0, 0, 0));
						
						sprintf(text, "%04X", val);
					}

					TextOut(mem_hdc, curx, cury, text, strlen(text));
					curx += (fontsize.cx * (4+1));
				}
				curx += (fontsize.cx * 6);
			}
			break;

		case 2:
			{
				u8 smallbuf[4];
				MMU_DumpMemBlock(wnd->region, wnd->selAddress, 4, smallbuf);
				char textbuf[32];
				sprintf(textbuf,"%f",((s32)T1ReadLong(smallbuf,0))/4096.0f);
				SetDlgItemText(wnd->hWnd,IDC_2012,textbuf);

				curx += (fontsize.cx * 8);
				for(i = 0; i < 16; i += 4)
				{
					u32 val = T1ReadLong(memory, ((line << 4) + i));
					if(IsDlgCheckboxChecked(wnd->hWnd,IDC_BIG_ENDIAN))
					{
						char swp[4];
						swp[0] = (val>>24)&0xFF;
						swp[1] = (val>>16)&0xFF;
						swp[2] = (val>>8)&0xFF;
						swp[3] = val&0xFF;
						val = *(u32*)swp;
					}
					if(wnd->sel && (wnd->selAddress == (addr + i)))
					{
						SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						
						switch(wnd->selPart)
						{
						case 0: sprintf(text, "%08X", val); break;
						case 1: sprintf(text, "%01X.......", wnd->selNewVal); break;
						case 2: sprintf(text, "%02X......", wnd->selNewVal); break;
						case 3: sprintf(text, "%03X.....", wnd->selNewVal); break;
						case 4: sprintf(text, "%04X....", wnd->selNewVal); break;
						case 5: sprintf(text, "%05X...", wnd->selNewVal); break;
						case 6: sprintf(text, "%06X..", wnd->selNewVal); break;
						case 7: sprintf(text, "%07X.", wnd->selNewVal); break;
						}
					}
					else
					{
						SetBkColor(mem_hdc, RGB(255, 255, 255));
						SetTextColor(mem_hdc, RGB(0, 0, 0));
						
						sprintf(text, "%08X", val);
					}	

					TextOut(mem_hdc, curx, cury, text, strlen(text));
					curx += (fontsize.cx * (8+1));
				}
				curx += (fontsize.cx * 8);
			}
			break;
		}

		SetBkColor(mem_hdc, RGB(255, 255, 255));
		SetTextColor(mem_hdc, RGB(0, 0, 0));

		for(i = 0; i < 16; i++)
		{
			u8 val = T1ReadByte(memory, ((line << 4) + i));

			if((val >= 32) && (val <= 127))
				text[i] = (char)val;
			else
				text[i] = '.';
		}
		text[16] = '\0';
		TextOut(mem_hdc, curx, cury, text, strlen(text));

		cury += fontsize.cy;
	}

	BitBlt(hdc, 0, 0, w, h, mem_hdc, 0, 0, SRCCOPY);

	DeleteDC(mem_hdc);
	DeleteObject(mem_bmp);

	EndPaint(hCtl, &ps);

	return 0;
}

LRESULT CALLBACK MemView_ViewBoxProc(HWND hCtl, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CMemView* wnd = (CMemView*)GetWindowLongPtr(hCtl, DWLP_USER);

	switch(uMsg)
	{
	case WM_NCCREATE:
		SetScrollRange(hCtl, SB_VERT, 0x00000000, 0x000FFFF0, TRUE);
		SetScrollPos(hCtl, SB_VERT, 0x00000000, TRUE);
		return 1;

	case WM_NCDESTROY:
		return 1;
	
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		MemView_ViewBoxPaint(wnd, hCtl, wParam, lParam);
		return 1;

	case WM_LBUTTONDOWN:
		{
			HDC hdc;
			HFONT font;
			SIZE fontsize;
			int x, y;

			wnd->sel = FALSE;
			wnd->selAddress = 0x00000000;
			wnd->selPart = 0;
			wnd->selNewVal = 0x00000000;

			hdc = GetDC(hCtl);
			font = (HFONT)SelectObject(hdc, wnd->font);
			GetTextExtentPoint32(hdc, " ", 1, &fontsize);
			
			x = LOWORD(lParam);
			y = HIWORD(lParam);

			if((x >= ((fontsize.cx * 8) + 5)) && (y >= (fontsize.cy + 3)))
			{
				int line, col;

				x -= ((fontsize.cx * 8) + 5);
				y -= (fontsize.cy + 3);
				
				line = (y / fontsize.cy);

				switch(wnd->viewMode)
				{
				case 0:
					{
						if((x < (fontsize.cx * 2)) || (x >= (fontsize.cx * (2 + ((2+1) * 16)))))
							break;

						col = ((x - (fontsize.cx * 2)) / (fontsize.cx * (2+1)));

						wnd->sel = TRUE;
						
					}
					break;

				case 1:
					{
						if((x < (fontsize.cx * 6)) || (x >= (fontsize.cx * (6 + ((4+1) * 8)))))
							break;

						col = ((x - (fontsize.cx * 6)) / (fontsize.cx * (4+1)) * 2);

						wnd->sel = TRUE;
						
					}
					break;

				case 2:
					{
						if((x < (fontsize.cx * 8)) || (x >= (fontsize.cx * (8 + ((8+1) * 4)))))
							break;

						col = ((x - (fontsize.cx * 8)) / (fontsize.cx * (8+1)) * 4);

						wnd->sel = TRUE;
					}
					break;
				}
				
				wnd->selAddress = (wnd->address + (line << 4) + col);
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;

			}

			SelectObject(hdc, font);
			ReleaseDC(hCtl, hdc);

			SetFocus(hCtl);				/* Required to receive keyboard messages */
			wnd->Refresh();
		}
		return 1;

	case WM_CHAR:
		{
			char ch = (char)wParam;

			if(((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F')) || ((ch >= 'a') && (ch <= 'f')))
			{
				if (!memIsAvailable((RegionType)wnd->region, wnd->selAddress))
					return DefWindowProc(hCtl, uMsg, wParam, lParam);

				u8 maxSelPart[3] = {2, 4, 8};

				wnd->selNewVal <<= 4;
				wnd->selPart++;

				if((ch >= '0') && (ch <= '9'))
					wnd->selNewVal |= (ch - '0');
				else if((ch >= 'A') && (ch <= 'F'))
					wnd->selNewVal |= (ch - 'A' + 0xA);
				else if((ch >= 'a') && (ch <= 'f'))
					wnd->selNewVal |= (ch - 'a' + 0xA);

				if(wnd->selPart >= maxSelPart[wnd->viewMode])
				{
					switch(wnd->viewMode)
					{
					case 0: memWrite8((RegionType)wnd->region, wnd->selAddress, (u8)wnd->selNewVal); wnd->selAddress++; break;
					case 1: memWrite16((RegionType)wnd->region, wnd->selAddress, (u16)wnd->selNewVal); wnd->selAddress += 2; break;
					case 2: memWrite32((RegionType)wnd->region, wnd->selAddress, wnd->selNewVal); wnd->selAddress += 4; break;
					}
					wnd->selPart = 0;
					wnd->selNewVal = 0x00000000;

					if(wnd->selAddress == 0x00000000)
					{
						wnd->sel = FALSE;
					}
					else if(wnd->selAddress >= (wnd->address + 0x100))
					{
						MemViewRegion& region = s_memoryRegions[wnd->region];
						HWAddressType addrMin = (region.hardwareAddress) & 0xFFFFFF00;
						HWAddressType addrMax = max(addrMin, (region.hardwareAddress + region.size - 0x100 - 1) & 0xFFFFFF00);
						if (wnd->address + 0x10 <= addrMax)
						{
							wnd->address += 0x10;
							SetScrollPos(hCtl, SB_VERT, (((wnd->address - region.hardwareAddress) >> 4) & 0x000FFFFF), TRUE);
						}
						else
						{
							switch(wnd->viewMode)
							{
							case 0: wnd->selAddress--; break;
							case 1: wnd->selAddress -= 2; break;
							case 2: wnd->selAddress -= 4; break;
							}
						}
					}
				}
			}

			wnd->Refresh();
		}
		return 1;

	case WM_VSCROLL:
		{
			int firstpos = GetScrollPos(hCtl, SB_VERT);
			MemViewRegion& region = s_memoryRegions[wnd->region];
			HWAddressType addrMin = (region.hardwareAddress) & 0xFFFFFF00;
			HWAddressType addrMax = (region.hardwareAddress + region.size - 1) & 0xFFFFFF00;

			switch(LOWORD(wParam))
			{
			case SB_LINEUP:
				wnd->address = (u32)max((int)addrMin, ((int)wnd->address - 0x10));
				break;

			case SB_LINEDOWN:
				wnd->address = min((u32)addrMax, (wnd->address + 0x10));
				break;

			case SB_PAGEUP:
				wnd->address = (u32)max((int)addrMin, ((int)wnd->address - 0x100));
				break;

			case SB_PAGEDOWN:
				wnd->address = min((u32)addrMax, (wnd->address + 0x100));
				break;

			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				{
					SCROLLINFO si;
					
					ZeroMemory(&si, sizeof(si));
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS;

					GetScrollInfo(hCtl, SB_VERT, &si);

					wnd->address = min((u32)addrMax, (wnd->address + ((si.nTrackPos - firstpos) * 16)));
				}
				break;
			}

			if((wnd->selAddress < wnd->address) || (wnd->selAddress >= (wnd->address + 0x100)))
			{
				wnd->sel = FALSE;
				wnd->selAddress = 0x00000000;
				wnd->selPart = 0;
				wnd->selNewVal = 0x00000000;
			}

			SetScrollPos(hCtl, SB_VERT, (((wnd->address - region.hardwareAddress) >> 4) & 0x000FFFFF), TRUE);
			wnd->Refresh();
		}
		return 1;
	}

	return DefWindowProc(hCtl, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
