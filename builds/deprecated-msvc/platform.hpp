#ifndef __PLATFORM_HPP_INCLUDED__
#define __PLATFORM_HPP_INCLUDED__

#define ZMQ_HAVE_WINDOWS

#if !defined(ZMQ_USE_CV_IMPL_NONE) && !defined(ZMQ_USE_CV_IMPL_WIN32API) && !defined(ZMQ_USE_CV_IMPL_STL11) && !defined(ZMQ_USE_CV_IMPL_VXWORKS) && !defined(ZMQ_USE_CV_IMPL_PTHREADS)
#define ZMQ_USE_CV_IMPL_WIN32API
#endif

// MSVC build configuration is controlled via options exposed in the Visual
// Studio user interface. The option to use libsodium is not exposed in the
// user interface unless a sibling `libsodium` directory to that of this
// repository exists and contains the following files:
//
// \builds\msvc\vs2015\libsodium.import.props
// \builds\msvc\vs2015\libsodium.import.xml

#endif
