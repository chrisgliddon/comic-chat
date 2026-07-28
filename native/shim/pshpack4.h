// pshpack4.h - MSVC struct-packing pragma header.
// The engine includes these around binary struct definitions; they must apply real
// packing, not be empty, or the .avb/.bmp structs would gain padding.
#pragma pack(push, 4)
