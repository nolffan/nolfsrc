// ----------------------------------------------------------------------- //
//
// MODULE  : p1_mousebutton.cpp
//
// PURPOSE : Package P1 (MODERN.md), M-P1-33: fixed names for the mouse
//           buttons.  See p1_mousebutton.h.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "p1_mousebutton.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
	// The buttons of one device, in the order DirectInput lists them.
	struct P1Buttons
	{
		int		m_nCount;
		DWORD	m_dwInstance[32];
		char	m_Name[32][MAX_PATH];
	};

	BOOL PASCAL CollectButton(LPCDIDEVICEOBJECTINSTANCEA lpddoi, LPVOID pvRef)
	{
		P1Buttons* pButtons = (P1Buttons*)pvRef;

		if (!IsEqualGUID(lpddoi->guidType, GUID_Button))
			return DIENUM_CONTINUE;

		if (pButtons->m_nCount >= 32)
			return DIENUM_STOP;

		pButtons->m_dwInstance[pButtons->m_nCount] = DIDFT_GETINSTANCE(lpddoi->dwType);
		strncpy(pButtons->m_Name[pButtons->m_nCount], lpddoi->tszName, MAX_PATH - 1);
		pButtons->m_Name[pButtons->m_nCount][MAX_PATH - 1] = '\0';
		pButtons->m_nCount++;

		return DIENUM_CONTINUE;
	}

	// The button's number: how many of the device's buttons have a lower
	// instance number.  -1 past the last fixed name.
	int ButtonRank(const P1Buttons* pButtons, int iButton)
	{
		int nRank = 0;
		int i;

		for (i = 0; i < pButtons->m_nCount; i++)
		{
			if (pButtons->m_dwInstance[i] < pButtons->m_dwInstance[iButton])
				nRank++;
		}

		return nRank < P1_MOUSE_BUTTONS ? nRank : -1;
	}

	// A whole decimal number of one or two digits, and nothing after it.
	BOOL ParseNumber(const char* p, int* pN)
	{
		if (p[0] < '0' || p[0] > '9')
			return FALSE;

		if (p[1] == '\0')
		{
			*pN = p[0] - '0';
			return TRUE;
		}

		if (p[1] < '0' || p[1] > '9' || p[2] != '\0')
			return FALSE;

		*pN = (p[0] - '0') * 10 + (p[1] - '0');
		return TRUE;
	}
}

BOOL P1Mouse_IsFixedName(const char* pName)
{
	int n;

	return pName && _strnicmp(pName, "##button", 8) == 0 && ParseNumber(pName + 8, &n);
}

void P1Mouse_FixedName(int nButton, char* pOut)
{
	sprintf(pOut, "##button%d", nButton);
}

int P1Mouse_ButtonNumber(LPDIRECTINPUTDEVICE2A pDevice, const char* pName,
                         char* pRealName, int nRealName)
{
	P1Buttons   buttons;
	const char* p;
	int         nWant = -1;
	int         i;

	if (!pDevice || !pName)
		return -1;

	buttons.m_nCount = 0;
	if (pDevice->EnumObjects(CollectButton, &buttons, DIDFT_BUTTON) != DI_OK)
		return -1;

	// The button's own name first: a config written by the retail game, or
	// by this build before M-P1-33, names it so.
	for (i = 0; i < buttons.m_nCount; i++)
	{
		if (_stricmp(buttons.m_Name[i], pName) == 0)
		{
			nWant = ButtonRank(&buttons, i);
			if (nWant < 0)
				return -1;
			break;
		}
	}

	if (nWant < 0)
	{
		// "##button<N>", the fixed name
		if (P1Mouse_IsFixedName(pName))
		{
			ParseNumber(pName + 8, &nWant);
		}
		else
		{
			// "Button <N>" or "##Button <N>", the names the retail configs
			// give (DirectInput's names on an English Windows)
			p = pName;
			if (p[0] == '#' && p[1] == '#')
				p += 2;

			if (_strnicmp(p, "Button ", 7) != 0 || !ParseNumber(p + 7, &nWant))
				return -1;
		}

		if (nWant >= P1_MOUSE_BUTTONS)
			return -1;
	}

	// The button with that number, if the mouse has it.
	for (i = 0; i < buttons.m_nCount; i++)
	{
		if (ButtonRank(&buttons, i) == nWant)
		{
			if (pRealName && nRealName > 0)
			{
				strncpy(pRealName, buttons.m_Name[i], nRealName - 1);
				pRealName[nRealName - 1] = '\0';
			}

			return nWant;
		}
	}

	return -1;
}


#ifdef NOLFSRC_DEBUG

// ----------------------------------------------------------------------- //
// The test of M-P1-33 (debug build only): a mouse whose buttons have other
// names.  Every call goes to the real device; only the buttons' names in
// EnumObjects and GetObjectInfo change.

namespace
{
	class CP1TestMouse : public IDirectInputDevice2A
	{
	public:
		CP1TestMouse(LPDIRECTINPUTDEVICEA pOrig, LPDIRECTINPUTDEVICE2A pReal, const char* pFormat)
		{
			m_nRef  = 1;
			m_pOrig = pOrig;
			m_pReal = pReal;
			strncpy(m_Format, pFormat, sizeof(m_Format) - 1);
			m_Format[sizeof(m_Format) - 1] = '\0';
		}

		// The name of a button, from its real description.
		void Rename(DIDEVICEOBJECTINSTANCEA* pdoi)
		{
			P1Buttons buttons;
			int i;

			if (!IsEqualGUID(pdoi->guidType, GUID_Button))
				return;

			buttons.m_nCount = 0;
			m_pReal->EnumObjects(CollectButton, &buttons, DIDFT_BUTTON);

			for (i = 0; i < buttons.m_nCount; i++)
			{
				if (buttons.m_dwInstance[i] == DIDFT_GETINSTANCE(pdoi->dwType))
				{
					_snprintf(pdoi->tszName, sizeof(pdoi->tszName), m_Format, ButtonRank(&buttons, i));
					pdoi->tszName[sizeof(pdoi->tszName) - 1] = '\0';
					return;
				}
			}
		}

		struct EnumContext
		{
			CP1TestMouse*					m_pThis;
			LPDIENUMDEVICEOBJECTSCALLBACKA	m_pCallback;
			LPVOID							m_pvRef;
		};

		static BOOL PASCAL EnumRenamed(LPCDIDEVICEOBJECTINSTANCEA lpddoi, LPVOID pvRef)
		{
			EnumContext* pContext = (EnumContext*)pvRef;
			DIDEVICEOBJECTINSTANCEA doi;

			memcpy(&doi, lpddoi, lpddoi->dwSize < sizeof(doi) ? lpddoi->dwSize : sizeof(doi));
			pContext->m_pThis->Rename(&doi);
			return pContext->m_pCallback(&doi, pContext->m_pvRef);
		}

		// IUnknown
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppv)
		{
			if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputDeviceA) ||
			    IsEqualGUID(riid, IID_IDirectInputDevice2A))
			{
				*ppv = (IDirectInputDevice2A*)this;
				AddRef();
				return S_OK;
			}

			*ppv = NULL;
			return E_NOINTERFACE;
		}

		STDMETHODIMP_(ULONG) AddRef()	{ return ++m_nRef; }

		STDMETHODIMP_(ULONG) Release()
		{
			ULONG n = --m_nRef;

			if (n == 0)
			{
				m_pReal->Release();
				m_pOrig->Release();
				delete this;
			}

			return n;
		}

		// IDirectInputDeviceA
		STDMETHODIMP GetCapabilities(LPDIDEVCAPS pCaps)	{ return m_pReal->GetCapabilities(pCaps); }

		STDMETHODIMP EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA pCallback, LPVOID pvRef, DWORD dwFlags)
		{
			EnumContext context;

			if (!pCallback)
				return DIERR_INVALIDPARAM;

			context.m_pThis     = this;
			context.m_pCallback = pCallback;
			context.m_pvRef     = pvRef;
			return m_pReal->EnumObjects(EnumRenamed, &context, dwFlags);
		}

		STDMETHODIMP GetProperty(REFGUID rguidProp, LPDIPROPHEADER pHeader)		{ return m_pReal->GetProperty(rguidProp, pHeader); }
		STDMETHODIMP SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pHeader)	{ return m_pReal->SetProperty(rguidProp, pHeader); }
		STDMETHODIMP Acquire()													{ return m_pReal->Acquire(); }
		STDMETHODIMP Unacquire()												{ return m_pReal->Unacquire(); }
		STDMETHODIMP GetDeviceState(DWORD cbData, LPVOID pvData)				{ return m_pReal->GetDeviceState(cbData, pvData); }

		STDMETHODIMP GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
		{
			return m_pReal->GetDeviceData(cbObjectData, rgdod, pdwInOut, dwFlags);
		}

		STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT pFormat)						{ return m_pReal->SetDataFormat(pFormat); }
		STDMETHODIMP SetEventNotification(HANDLE hEvent)						{ return m_pReal->SetEventNotification(hEvent); }
		STDMETHODIMP SetCooperativeLevel(HWND hWnd, DWORD dwFlags)				{ return m_pReal->SetCooperativeLevel(hWnd, dwFlags); }

		STDMETHODIMP GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA pdoi, DWORD dwObj, DWORD dwHow)
		{
			HRESULT hResult = m_pReal->GetObjectInfo(pdoi, dwObj, dwHow);

			if (hResult == DI_OK && pdoi)
				Rename(pdoi);
			return hResult;
		}

		STDMETHODIMP GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi)					{ return m_pReal->GetDeviceInfo(pdidi); }
		STDMETHODIMP RunControlPanel(HWND hWnd, DWORD dwFlags)					{ return m_pReal->RunControlPanel(hWnd, dwFlags); }
		STDMETHODIMP Initialize(HINSTANCE hInst, DWORD dwVersion, REFGUID rguid)	{ return m_pReal->Initialize(hInst, dwVersion, rguid); }

		// IDirectInputDevice2A
		STDMETHODIMP CreateEffect(REFGUID rguid, LPCDIEFFECT pEffect, LPDIRECTINPUTEFFECT* ppEffect, LPUNKNOWN pOuter)
		{
			return m_pReal->CreateEffect(rguid, pEffect, ppEffect, pOuter);
		}

		STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA pCallback, LPVOID pvRef, DWORD dwType)
		{
			return m_pReal->EnumEffects(pCallback, pvRef, dwType);
		}

		STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA pInfo, REFGUID rguid)		{ return m_pReal->GetEffectInfo(pInfo, rguid); }
		STDMETHODIMP GetForceFeedbackState(LPDWORD pdwOut)						{ return m_pReal->GetForceFeedbackState(pdwOut); }
		STDMETHODIMP SendForceFeedbackCommand(DWORD dwFlags)					{ return m_pReal->SendForceFeedbackCommand(dwFlags); }

		STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK pCallback, LPVOID pvRef, DWORD dwFlags)
		{
			return m_pReal->EnumCreatedEffectObjects(pCallback, pvRef, dwFlags);
		}

		STDMETHODIMP Escape(LPDIEFFESCAPE pEscape)								{ return m_pReal->Escape(pEscape); }
		STDMETHODIMP Poll()														{ return m_pReal->Poll(); }

		STDMETHODIMP SendDeviceData(DWORD cbObjectData, LPCDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags)
		{
			return m_pReal->SendDeviceData(cbObjectData, rgdod, pdwInOut, dwFlags);
		}

	private:
		ULONG					m_nRef;
		LPDIRECTINPUTDEVICEA	m_pOrig;
		LPDIRECTINPUTDEVICE2A	m_pReal;
		char					m_Format[64];
	};
}

LPDIRECTINPUTDEVICEA P1Mouse_TestWrap(REFGUID guid, LPDIRECTINPUTDEVICEA pDevice)
{
	const char*				pFormat = getenv("NOLFSRC_TEST_MOUSENAMES");
	LPDIRECTINPUTDEVICE2A	pReal   = NULL;

	if (!pDevice || !pFormat || !pFormat[0] || !IsEqualGUID(guid, GUID_SysMouse))
		return pDevice;

	if (pDevice->QueryInterface(IID_IDirectInputDevice2A, (void**)&pReal) != S_OK || !pReal)
		return pDevice;

	return (LPDIRECTINPUTDEVICEA)(IDirectInputDevice2A*)new CP1TestMouse(pDevice, pReal, pFormat);
}

#endif // NOLFSRC_DEBUG
