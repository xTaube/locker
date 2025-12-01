#ifndef ATTRS_H
#define ATTRS_H

#ifdef __GNUC__
#define ATTR_ALLOC __attribute__((malloc))
#define ATTR_NODISCARD __attribute__((warn_unused_result))
#define ATTR_PURE __attribute__((pure))
#else
#define ATTR_ALLOC
#define ATTR_NODISCARD
#define ATTR_PURE
#endif

#endif
