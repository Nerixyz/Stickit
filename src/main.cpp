#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers

// clang-format off
#include <windows.h>
#include <windowsx.h>
#include <winrt/base.h>
#include <wincodec.h>
#include <d2d1.h>
#include <d2d1helper.h>
// clang-format on

#include "resource.h"

#include <expected>

constexpr float DEFAULT_DPI = 96.F;
constexpr LONG BUTTON_SIZE = 32;
constexpr LONG BUTTON_BORDER = 8;

#define BAIL(hr)                                                               \
  if (!SUCCEEDED((hr))) {                                                      \
    return hr;                                                                 \
  }

struct Size {
  UINT width;
  UINT height;
};

LRESULT CALLBACK globalWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                               LPARAM lParam);

class Stickit {
public:
  Stickit() = default;
  ~Stickit() = default;

  HRESULT Initialize(HINSTANCE hInstance);
  void showNow();

  LRESULT wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  [[nodiscard]] HWND hwnd() const { return this->hWnd; }

private:
  HRESULT createD2DBitmapFromHBITMAP(HBITMAP hBitmap);
  HRESULT createDeviceResources();

  LRESULT onPaint(HWND hWnd);
  void onSize(LPARAM lParam);
  void onSizing(WPARAM wParam, LPARAM lParam) const;
  LRESULT onHittest(LPARAM lParam);

  HWND hWnd = nullptr;
  HINSTANCE hInst = nullptr;
  HCURSOR sizeAllCursor = nullptr;

  Size originalSize;

  bool hoveringButtons = false;

  winrt::com_ptr<IWICImagingFactory> wicFactory;
  winrt::com_ptr<IWICFormatConverter> formatConverter;

  winrt::com_ptr<ID2D1Factory> d2dFactory;
  winrt::com_ptr<ID2D1HwndRenderTarget> renderTarget;
  winrt::com_ptr<ID2D1Bitmap> bitmap;

  winrt::com_ptr<ID2D1SolidColorBrush> redBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> whiteBrush;
  winrt::com_ptr<ID2D1SolidColorBrush> greyBrush;
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                    LPWSTR /*lpCmdLine*/, int /*nShowCmd*/) {
  HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, NULL, 0);

  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                           COINIT_DISABLE_OLE1DDE);

  if (!SUCCEEDED(hr)) {
    return -1;
  }
  Stickit app;
  if (!SUCCEEDED(app.Initialize(hInstance))) {
    CoUninitialize();
    return -1;
  }

  app.showNow();

  HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));

  BOOL fRet;
  MSG msg;
  while ((fRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
    if (fRet == -1) {
      break;
    }

    if (!TranslateAccelerator(app.hwnd(), hAccel, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  CoUninitialize();

  return 0;
}

std::expected<HBITMAP, HRESULT> readClipboard(HWND hwnd) {
  if (OpenClipboard(hwnd) == FALSE) {
    return std::unexpected{E_FAIL};
  }

  auto *bitmap = reinterpret_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
  CloseClipboard();

  if (bitmap != nullptr) {
    return bitmap;
  }
  return std::unexpected{E_FAIL};
}

HRESULT Stickit::Initialize(HINSTANCE hInstance) {

  HRESULT hr = S_OK;

  BAIL(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                        IID_PPV_ARGS(&this->wicFactory)));

  BAIL(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                         this->d2dFactory.put()));

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  this->sizeAllCursor = LoadCursor(nullptr, IDC_SIZEALL);

  WNDCLASSEX wcex;

  // Register window class
  wcex.cbSize = sizeof(WNDCLASSEX);
  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = globalWndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = sizeof(LONG_PTR);
  wcex.hInstance = hInstance;
  wcex.hIcon = nullptr;
  wcex.hCursor = this->sizeAllCursor;
  wcex.hbrBackground = nullptr;
  wcex.lpszClassName = L"StickitCls";
  wcex.hIconSm = nullptr;
  wcex.lpszMenuName = nullptr;

  this->hInst = hInstance;

  BAIL(RegisterClassEx(&wcex) ? S_OK : E_FAIL);

  this->hWnd = CreateWindow(L"StickitCls", L"Stickit", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, NULL, NULL,
                            hInstance, this);

  BAIL(this->hWnd ? S_OK : E_FAIL);

  auto clipboard = readClipboard(hWnd);
  while (!clipboard) {
    auto ret = MessageBox(this->hWnd, L"No image in clipboard", L"Stickit",
                          MB_ICONERROR | MB_RETRYCANCEL);
    if (ret == IDRETRY) {
      clipboard = readClipboard(hWnd);
      continue;
    }
    return clipboard.error();
  }
  this->createD2DBitmapFromHBITMAP(*clipboard);
  DeleteObject(*clipboard);

  return hr;
}

void Stickit::showNow() {
  ShowWindow(this->hWnd, SW_SHOW);
  SetWindowPos(this->hWnd, HWND_TOPMOST, 0, 0,
               static_cast<int>(this->originalSize.width),
               static_cast<int>(this->originalSize.height), SWP_NOMOVE);
}

HRESULT Stickit::createD2DBitmapFromHBITMAP(HBITMAP hBitmap) {
  winrt::com_ptr<IWICBitmapDecoder> pDecoder;
  winrt::com_ptr<IWICBitmap> wicBitmap;

  BAIL(this->wicFactory->CreateBitmapFromHBITMAP(
      hBitmap, nullptr, WICBitmapUseAlpha, wicBitmap.put()));

  BAIL(wicBitmap->GetSize(&this->originalSize.width,
                          &this->originalSize.height));

  BAIL(this->wicFactory->CreateFormatConverter(this->formatConverter.put()));
  BAIL(this->formatConverter->Initialize(
      wicBitmap.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
      nullptr, 0.0, WICBitmapPaletteTypeCustom));

  BAIL(this->createDeviceResources());

  this->bitmap = nullptr;
  BAIL(this->renderTarget->CreateBitmapFromWicBitmap(
      this->formatConverter.get(), NULL, this->bitmap.put()));

  return S_OK;
}

HRESULT Stickit::createDeviceResources() {
  if (!this->renderTarget) {
    RECT rc;
    BAIL(GetClientRect(this->hWnd, &rc) ? S_OK : E_FAIL);

    // Create a D2D render target properties
    D2D1_RENDER_TARGET_PROPERTIES renderTargetProperties =
        D2D1::RenderTargetProperties();

    // Set the DPI to be the default system DPI to allow direct mapping
    // between image pixels and desktop pixels in different system DPI
    // settings
    renderTargetProperties.dpiX = DEFAULT_DPI;
    renderTargetProperties.dpiY = DEFAULT_DPI;

    // Create a D2D render target
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    BAIL(this->d2dFactory->CreateHwndRenderTarget(
        renderTargetProperties,
        D2D1::HwndRenderTargetProperties(this->hWnd, size),
        this->renderTarget.put()));
    this->renderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    auto makeBrush = [&](auto col, auto &target) {
      return this->renderTarget->CreateSolidColorBrush(D2D1::ColorF(col),
                                                       target.put());
    };
    BAIL(makeBrush(D2D1::ColorF::Red, this->redBrush));
    BAIL(makeBrush(D2D1::ColorF::White, this->whiteBrush));
    BAIL(makeBrush(0x212121, this->greyBrush));
  }

  return S_OK;
}

LRESULT CALLBACK globalWndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                               LPARAM lParam) {

  if (uMsg == WM_NCCREATE) {
    auto *pcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
    auto *self = reinterpret_cast<Stickit *>(pcs->lpCreateParams);

    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
  }

  auto *self =
      reinterpret_cast<Stickit *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
  LRESULT lRet = 0;
  if (self != nullptr) {
    lRet = self->wndProc(hWnd, uMsg, wParam, lParam);
  } else {
    lRet = DefWindowProc(hWnd, uMsg, wParam, lParam);
  }
  return lRet;
}

LRESULT Stickit::wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  switch (uMsg) {
  case WM_SIZE: {
    this->onSize(lParam);
    break;
  }

  case WM_SIZING: { // ensure correct aspect ratio
    this->onSizing(wParam, lParam);
    return TRUE;
  }

  case WM_NCCALCSIZE: { // disable window border
    if (wParam == TRUE) {
      auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lParam);
      params->lppos->flags |= SWP_NOREDRAW;
    }

    return 0;
  }

  case WM_NCHITTEST: { // handle resize handles, close & minimize, and moving
    return this->onHittest(lParam);
  }

  case WM_NCLBUTTONDOWN: { // disable default [fallback] buttons
    if (wParam == HTMINBUTTON || wParam == HTCLOSE) {
      return 0;
    }
    break;
  }

  case WM_NCLBUTTONUP: {
    if (wParam == HTMINBUTTON) {
      ShowWindow(hWnd, SW_MINIMIZE);
      return 0;
    }
    if (wParam == HTCLOSE) {
      PostQuitMessage(0);
      return 0;
    }
    break;
  }

  case WM_SETCURSOR: { // disable default cursor
    if (LOWORD(lParam) == HTCAPTION) {
      SetCursor(this->sizeAllCursor);
      return TRUE;
    }
    break;
  }

  case WM_PAINT: {
    return onPaint(hWnd);
  }

  case WM_COMMAND: {
    if (LOWORD(wParam) == ID_CLOSE_WINDOW) {
      PostQuitMessage(0);
      return 0;
    }
    break;
  }

  case WM_DESTROY: {
    PostQuitMessage(0);
    return 0;
  }

  default:;
  }

  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Stickit::onSize(LPARAM lParam) {
  D2D1_SIZE_U size = D2D1::SizeU(LOWORD(lParam), HIWORD(lParam));

  if (this->renderTarget) {
    // If we couldn't resize, release the device and we'll recreate it
    // during the next render pass.
    if (FAILED(this->renderTarget->Resize(size))) {
      this->renderTarget = nullptr;
      this->bitmap = nullptr;
    }
  }
}

void Stickit::onSizing(WPARAM wParam, LPARAM lParam) const {
  auto *rect = reinterpret_cast<RECT *>(lParam);
  auto width = rect->right - rect->left;
  auto height = rect->bottom - rect->top;

  enum Adapt { Width, Height };
  enum Origin { TopLeft, BottomRight };
  Adapt a;
  Origin o;

  auto fix = [&](Adapt a, Origin o) {
    if (a == Width) {
      width = static_cast<long>(this->originalSize.width) * height /
              static_cast<long>(this->originalSize.height);
      if (o == TopLeft) {
        rect->right = rect->left + width;
      } else {
        rect->left = rect->right - width;
      }
    } else {
      height = static_cast<long>(this->originalSize.height) * width /
               static_cast<long>(this->originalSize.width);
      if (o == TopLeft) {
        rect->bottom = rect->top + height;
      } else {
        rect->top = rect->bottom - height;
      }
    }
  };

  switch (wParam) {
  case WMSZ_BOTTOM:
    fix(Width, TopLeft);
    break;
  case WMSZ_BOTTOMLEFT:
  case WMSZ_BOTTOMRIGHT:
    fix(Height, TopLeft);
    break;
  case WMSZ_LEFT:
    fix(Width, BottomRight);
    break;
  case WMSZ_RIGHT:
  case WMSZ_TOP:
    fix(Width, TopLeft);
    break;
  case WMSZ_TOPLEFT:
  case WMSZ_TOPRIGHT:
    fix(Height, BottomRight);
    break;
  default:;
  }
}

LRESULT Stickit::onHittest(LPARAM lParam) {
  constexpr LONG borderWidth = 8;
  RECT winrect;
  GetWindowRect(hWnd, &winrect);

  long x = GET_X_LPARAM(lParam);
  long y = GET_Y_LPARAM(lParam);

  // top right corner
  if (y <= winrect.top + BUTTON_SIZE && x >= winrect.right - 2 * BUTTON_SIZE) {
    auto btn = HTMINBUTTON;
    if (x >= winrect.right - BUTTON_SIZE) {
      btn = HTCLOSE;
    }

    if (!this->hoveringButtons) {
      this->hoveringButtons = true;
      InvalidateRect(hWnd, nullptr, TRUE);
    }
    return btn;
  }

  if (this->hoveringButtons) {
    this->hoveringButtons = false;
    InvalidateRect(hWnd, nullptr, TRUE);
  }

  // bottom left corner
  if (x >= winrect.left && x < winrect.left + borderWidth &&
      y < winrect.bottom && y >= winrect.bottom - borderWidth) {
    return HTBOTTOMLEFT;
  }
  // bottom right corner
  if (x < winrect.right && x >= winrect.right - borderWidth &&
      y < winrect.bottom && y >= winrect.bottom - borderWidth) {
    return HTBOTTOMRIGHT;
  }
  // top left corner
  if (x >= winrect.left && x < winrect.left + borderWidth && y >= winrect.top &&
      y < winrect.top + borderWidth) {
    return HTTOPLEFT;
  }
  // top right corner
  if (x < winrect.right && x >= winrect.right - borderWidth &&
      y >= winrect.top && y < winrect.top + borderWidth) {
    return HTTOPRIGHT;
  }

  // left border
  if (x < winrect.left + borderWidth) {
    return HTLEFT;
  }
  // right border
  if (x >= winrect.right - borderWidth) {
    return HTRIGHT;
  }

  // bottom border
  if (y >= winrect.bottom - borderWidth) {
    return HTBOTTOM;
  }
  // top border
  if (y < winrect.top + borderWidth) {
    return HTTOP;
  }
  return HTCAPTION;
}

LRESULT Stickit::onPaint(HWND hWnd) {
  HRESULT hr = S_OK;
  PAINTSTRUCT ps;

  if (BeginPaint(hWnd, &ps) != nullptr) {
    // create render target if not yet created
    hr = createDeviceResources();

    if (SUCCEEDED(hr) && (this->renderTarget->CheckWindowState() &
                          D2D1_WINDOW_STATE_OCCLUDED) == 0) {
      this->renderTarget->BeginDraw();

      this->renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

      this->renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

      D2D1_SIZE_F size = this->renderTarget->GetSize();

      // recreate in case of device loss
      if (this->formatConverter && !this->bitmap) {
        this->renderTarget->CreateBitmapFromWicBitmap(
            this->formatConverter.get(), nullptr, this->bitmap.put());
      }

      if (this->bitmap) {
        this->renderTarget->DrawBitmap(
            this->bitmap.get(),
            D2D1::RectF(0.0F, 0.0F, size.width, size.height));
      }

      // super basic buttons
      if (this->hoveringButtons) {
        // close button
        this->renderTarget->FillRectangle(
            D2D1::RectF(size.width - BUTTON_SIZE, 0, size.width, BUTTON_SIZE),
            this->redBrush.get());

        this->renderTarget->DrawLine(
            D2D1::Point2F(size.width - BUTTON_SIZE + BUTTON_BORDER,
                          BUTTON_BORDER),
            D2D1::Point2F(size.width - BUTTON_BORDER,
                          BUTTON_SIZE - BUTTON_BORDER),
            this->whiteBrush.get());
        this->renderTarget->DrawLine(
            D2D1::Point2F(size.width - BUTTON_BORDER, BUTTON_BORDER),
            D2D1::Point2F(size.width - BUTTON_SIZE + BUTTON_BORDER,
                          BUTTON_SIZE - BUTTON_BORDER),
            this->whiteBrush.get());

        // minimize
        this->renderTarget->FillRectangle(
            D2D1::RectF(size.width - 2 * BUTTON_SIZE, 0,
                        size.width - BUTTON_SIZE, BUTTON_SIZE),
            this->greyBrush.get());
        this->renderTarget->DrawLine(
            D2D1::Point2F(size.width - 2 * BUTTON_SIZE + BUTTON_BORDER,
                          BUTTON_SIZE >> 1),
            D2D1::Point2F(size.width - BUTTON_SIZE - BUTTON_BORDER,
                          BUTTON_SIZE >> 1),
            this->whiteBrush.get());
      }

      hr = this->renderTarget->EndDraw();

      // In case of device loss, discard D2D render target and D2DBitmap
      // They will be re-created in the next rendering pass
      if (hr == D2DERR_RECREATE_TARGET) {
        this->bitmap = nullptr;
        this->renderTarget = nullptr;
        // Force a re-render
        hr = InvalidateRect(hWnd, nullptr, TRUE) == TRUE ? S_OK : E_FAIL;
      }
    }

    EndPaint(hWnd, &ps);
  }

  return SUCCEEDED(hr) ? FALSE : TRUE;
}
