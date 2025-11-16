#ifndef __DEFINES_HPP__
#define __DEFINES_HPP__

#ifdef DEBUG
#define RETURN_DEBUG(...) return __VA_ARGS__
#define RETURN_NOT_DEBUG(...)

#define IF_DEBUG(...) __VA_ARGS__
#define IF_NOT_DEBUG(...)
#else
#define RETURN_DEBUG(...)
#define RETURN_NOT_DEBUG(...) return __VA_ARGS__

#define IF_NOT_DEBUG(...) __VA_ARGS__
#define IF_DEBUG(...)
#endif

// json utils
#define IsType(obj, key, type)  (obj.HasMember(key) && obj[key].Is##type())
#define NotType(obj, key, type) (!obj.HasMember(key) || !obj[key].Is##type())
#define Exists(obj, key)        (obj.HasMember(key) && !obj[key].IsNull())

#endif