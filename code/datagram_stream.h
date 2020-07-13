#if !defined(DATAGRAM_STREAM)

#define UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE (576 - (8 + 60))
//#define UDP_MAX_OS_PAYLOAD_SIZE (0xFFFF - (8 + 60))

struct datagram
{
  u8 Payload[UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE];
  memi Used;
  memi UnpackOffset;
};

internal_function void
ClearDatagram(datagram* Datagram)
{
  Datagram->Used = 0;
  Datagram->UnpackOffset = 0;
}

struct datagram_stream
{
  u32 DatagramCount;
  datagram* Datagrams;
  b32 Complete;
};

#define DATAGRAM_TYPE_INPUT 0
#define DATAGRAM_TYPE_RENDER_GROUP 1

#define PackData(datagram, data) PackData_((datagram), (u8*)(data), sizeof(*data))
#define PackBuffer(datagram, data, size) PackData_((datagram), (data), (size))
//TODO(bjorn): Byte-order.
internal_function void
PackData_(datagram* Datagram, u8* Data, memi Size)
{
  Assert(Datagram->Used + Size <= UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE);
  RtlCopyMemory(Datagram->Payload + Datagram->Used, Data, Size);
  Datagram->Used += Size;
}
#define GenPackVarByType(type) \
internal_function void \
PackVar(datagram* Datagram, type Var) \
{ \
  memi Size = sizeof(type); \
  type* Data = &Var; \
 \
  PackData_(Datagram, Data, Size); \
}

#define UnpackData(datagram, destination) UnpackData_((datagram), (u8*)(destination), sizeof(*destination))
#define UnpackBuffer(datagram, destination, size) UnpackData_((datagram), (destination), (size))
internal_function void
UnpackData_(datagram* Datagram, u8* Destination, memi Size)
{
  Assert(Datagram->UnpackOffset < Datagram->Used);
  Assert(Size <= Datagram->Used - Datagram->UnpackOffset);
  RtlCopyMemory(Destination, Datagram->Payload + Datagram->UnpackOffset, Size);
  Datagram->UnpackOffset += Size;
}
#define GenUnpackVarByType(type) \
internal_function type \
UnpackVar(datagram* Datagram) \
{ \
  type Result; \
  memi Size = sizeof(type); \
 \
  UnpackData_(Datagram, (u8*)&Result, Size); \
 \
  return Result; \
}

#define GenPackUnpackVarByType(type) GenPackVarByType(type) GenUnpackVarByType(type)
GenPackUnpackVarByType(u8);

#define DATAGRAM_STREAM
#endif
