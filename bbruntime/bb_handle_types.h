
#ifndef BB_HANDLE_TYPES_H
#define BB_HANDLE_TYPES_H

// Type-specific specialisations for handle_family and handle_type_tag.
// Include this AFTER the real type definitions (blitz3d headers, gxruntime headers).
// The primary templates are in bb_handle.h; this file adds the specialisations.

#include "bb_handle.h"

#ifndef _WIN32

// ---- Entity family ----
class Entity;
class Object;
class Model;
class MeshModel;
class Camera;
class Light;
class Sprite;
class MD2Model;
class Q3BSPModel;
class Terrain;
class Pivot;
class Mirror;
class Listener;

template <> struct handle_family<Entity>     { typedef Entity base; };
template <> struct handle_family<Object>     { typedef Entity base; };
template <> struct handle_family<Model>      { typedef Entity base; };
template <> struct handle_family<MeshModel>  { typedef Entity base; };
template <> struct handle_family<Camera>     { typedef Entity base; };
template <> struct handle_family<Light>      { typedef Entity base; };
template <> struct handle_family<Sprite>     { typedef Entity base; };
template <> struct handle_family<MD2Model>   { typedef Entity base; };
template <> struct handle_family<Q3BSPModel> { typedef Entity base; };
template <> struct handle_family<Terrain>    { typedef Entity base; };
template <> struct handle_family<Pivot>      { typedef Entity base; };
template <> struct handle_family<Mirror>     { typedef Entity base; };
template <> struct handle_family<Listener>   { typedef Entity base; };

template <> struct handle_type_tag<Entity>     { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Object>     { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Model>      { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<MeshModel>  { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Camera>     { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Light>      { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Sprite>     { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<MD2Model>   { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Q3BSPModel> { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Terrain>    { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Pivot>      { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Mirror>     { static const int value = HT_ENTITY; };
template <> struct handle_type_tag<Listener>   { static const int value = HT_ENTITY; };

// ---- Stream family ----
class bbStream;
struct bbFile;
class TCPStream;
class UDPStream;

template <> struct handle_family<bbStream>  { typedef bbStream base; };
template <> struct handle_family<bbFile>    { typedef bbStream base; };
template <> struct handle_family<TCPStream> { typedef bbStream base; };
template <> struct handle_family<UDPStream> { typedef bbStream base; };

template <> struct handle_type_tag<bbStream>  { static const int value = HT_STREAM; };
template <> struct handle_type_tag<bbFile>    { static const int value = HT_STREAM; };
template <> struct handle_type_tag<TCPStream> { static const int value = HT_STREAM; };
template <> struct handle_type_tag<UDPStream> { static const int value = HT_STREAM; };

// ---- Singletons / standalone ----
class TCPServer;
class Texture;
class Brush;
class Surface;
class bbImage;
class gxCanvas;
class gxFont;
class gxMovie;
class gxDir;
struct bbBank;
struct Sound;
class gxSound;
class gxTimer;

template <> struct handle_type_tag<TCPServer> { static const int value = HT_TCP_SERVER; };
template <> struct handle_type_tag<Texture>   { static const int value = HT_TEXTURE; };
template <> struct handle_type_tag<Brush>     { static const int value = HT_BRUSH; };
template <> struct handle_type_tag<Surface>   { static const int value = HT_SURFACE; };
template <> struct handle_type_tag<bbImage>   { static const int value = HT_IMAGE; };
template <> struct handle_type_tag<gxCanvas>  { static const int value = HT_CANVAS; };
template <> struct handle_type_tag<gxFont>    { static const int value = HT_FONT; };
template <> struct handle_type_tag<gxMovie>   { static const int value = HT_MOVIE; };
template <> struct handle_type_tag<gxDir>     { static const int value = HT_DIR; };
template <> struct handle_type_tag<bbBank>    { static const int value = HT_BANK; };
template <> struct handle_type_tag<Sound>     { static const int value = HT_SOUND; };
template <> struct handle_type_tag<gxSound>   { static const int value = HT_SOUND; };
template <> struct handle_type_tag<gxTimer>   { static const int value = HT_TIMER; };

// ---- Sound family (both backends) ----
template <> struct handle_family<Sound>   { typedef Sound base; };
template <> struct handle_family<gxSound> { typedef gxSound base; };

#endif // _WIN32

#endif // BB_HANDLE_TYPES_H
