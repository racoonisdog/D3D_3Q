#ifdef _MSC_VER
#pragma once
#endif

/* Used to reduce to reduce the number of lines when calling Release()
   on a D3D interface. Implemented as a template instead of a 'SAFE_RELEASE'
   MACRO to ease debugging. */
template<typename T>
inline void SafeRelease(T*& x) {
    if (x) {
        x->Release();
        x = nullptr;
    }
}