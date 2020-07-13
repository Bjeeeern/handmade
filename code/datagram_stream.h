#if !defined(DATAGRAM_STREAM)

#define UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE (576 - (8 + 60))
//#define UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE 4046
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
  u8 Generation;

  u8 ConcurrentGenerationCount;
  u8 StreamDatagramLength;
  datagram* Datagrams;

  b32 Complete;

  u8 CurrentPayloadDatagramLenght;
};

internal_function datagram*
GetDatagram(datagram_stream* Stream, u8 DatagramIndex, u8 GenerationIndex = 0)
{
  Assert(DatagramIndex < Stream->StreamDatagramLength);
  Assert(GenerationIndex < Stream->ConcurrentGenerationCount);
  return Stream->Datagrams + (GenerationIndex * Stream->StreamDatagramLength + DatagramIndex);
}

internal_function void
ClearDatagramStream(datagram_stream* Stream, u8 GenerationIndex = 0)
{
  Stream->CurrentPayloadDatagramLenght = 0;

  for(u8 DatagramIndex = 0;
      DatagramIndex < Stream->StreamDatagramLength;
      DatagramIndex++)
  {
    datagram* Datagram = GetDatagram(Stream, DatagramIndex, GenerationIndex);
    ClearDatagram(Datagram);
  }
}

internal_function datagram_stream*
CreateDatagramStream(memory_arena* Arena, u8 StreamDatagramLength, u8 ConcurrentGenerationCount)
{
  datagram_stream* Result = PushStruct(Arena, datagram_stream);

  Result->ConcurrentGenerationCount = ConcurrentGenerationCount;
  Result->StreamDatagramLength = StreamDatagramLength;

  Result-> Generation = 0;
  Result->Complete = false;

  Result->Datagrams = PushArray(Arena, StreamDatagramLength * ConcurrentGenerationCount, datagram);

  ClearDatagramStream(Result);
}

internal_function void
PackData_(datagram* Datagram, u8* Data, memi Size)
{
  Assert(Datagram->Used + Size <= UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE);
  RtlCopyMemory(Datagram->Payload + Datagram->Used, Data, Size);
  Datagram->Used += Size;
}

#define PackData(datagram_stream, data) PackData_((datagram_stream), (u8*)(data), sizeof(*data))
#define PackBuffer(datagram_stream, data, size) PackData_((datagram_stream), (data), (size))
//TODO(bjorn): Byte-order.
internal_function void
PackData_(datagram_stream* DatagramStream, u8* Data, memi Size)
{
  Assert(Size);

  memi SizeLeft = Size;
  for(u8 DatagramIndex = 0;
      DatagramIndex < DatagramStream->StreamDatagramLength;
      DatagramIndex++)
  {
    datagram* Datagram = GetDatagram(DatagramStream, DatagramIndex);
    if(Datagram->Used == UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE)
    {
      continue;
    }

    if(Datagram->Used == 0)
    {
      u8 Var = DatagramStream->Generation;
      PackData_(Datagram, &Var, sizeof(u8));

      Var = DatagramIndex;
      PackData_(Datagram, &Var, sizeof(u8));

      if(DatagramIndex == 0)
      {
        //NOTE(bjorn): Dummy value to be filled in later when the whole stream has been packed
        //             and will be sent.
        Var = 0;
        PackData_(Datagram, &Var, sizeof(u8));
      }

      DatagramStream->CurrentPayloadDatagramLenght += 1;
    }

    memi AvailableSpace = UDP_NON_FRAGMENTABLE_PAYLOAD_SIZE - Datagram->Used;
    if(SizeLeft <= AvailableSpace)
    {
      PackData_(Datagram, Data, SizeLeft);
      SizeLeft = 0;

      break;
    }
    else
    {
      PackData_(Datagram, Data, AvailableSpace);
      SizeLeft -= AvailableSpace;
      Data += AvailableSpace;
    }

    Assert(SizeLeft);
  }

  Assert(SizeLeft == 0);
}
#define GenPackVarByType(type) \
internal_function void \
PackVar(datagram_stream* DatagramStream, type Var) \
{ \
  memi Size = sizeof(type); \
  type* Data = &Var; \
 \
  PackData_(DatagramStream, Data, Size); \
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
