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

struct gen_meta_data
{
  b32 Complete;
  u8 FoundDgramCount;
  u8 TotalDgramCount;
  u8 Generation;
};

//TODO(bjorn): Maybe have two stream structs? One for sending and one for receiving.
struct datagram_stream
{
  u8 ConcurrentGenerationCount;
  u8 StreamDatagramLength;
  datagram* Datagrams;

  //NOTE IN
  gen_meta_data* GenMeta;

  //NOTE OUT
  u8 CurrentPayloadDatagramLenght;
  u8 Generation;
};

internal_function datagram*
GetDatagram(datagram_stream* Stream, u8 DatagramIndex, u8 GenerationIndex = 0)
{
  Assert(DatagramIndex < Stream->StreamDatagramLength);
  Assert(GenerationIndex < Stream->ConcurrentGenerationCount);
  return Stream->Datagrams + (GenerationIndex * Stream->StreamDatagramLength + DatagramIndex);
}

inline b32
AOlderB(u8 A, u8 B)
{
  s32 Diff = B - A;

  if(Diff == 0)
  {
    return false;
  }

  if(AbsoluteS32(Diff) < (max_u8 / 2))
  {
    return Diff > 0;
  }
  else
  {
    return Diff < 0;
  }
}

internal_function void
AddDatagramToStream(datagram* Datagram, datagram_stream* Stream)
{
  u8 Generation = Datagram->Payload[0];
  u8 DatagramIndex = Datagram->Payload[1];
  u8 TotalDgrams = 0;
  if(DatagramIndex == 0)
  {
    TotalDgrams = Datagram->Payload[2];
  }

  u8 OldestIncompleteGenIndex = 0;
  u8 OldestGenIndex = 0;
  for(u8 GenerationIndex = 0;
      GenerationIndex < Stream->ConcurrentGenerationCount;
      GenerationIndex++)
  {
    gen_meta_data* Meta = Stream->GenMeta + GenerationIndex;
    if(Meta->Generation == Generation)
    {
      *GetDatagram(Stream, DatagramIndex, GenerationIndex) = *Datagram;
      Meta->FoundDgramCount += 1;

      if(DatagramIndex == 0)
      {
        Meta->TotalDgramCount = TotalDgrams;
      }
      if(Meta->FoundDgramCount == Meta->TotalDgramCount)
      {
        Meta->Complete = true;
      }
      return;
    }
    else
    {
      if(AOlderB(Meta->Generation, Stream->GenMeta[OldestGenIndex].Generation))
      {
        OldestGenIndex = GenerationIndex;
      }
      //TODO(Bjorn): Double-check that this makes sense.
      if((!Meta->Complete && 
          AOlderB(Meta->Generation, Stream->GenMeta[OldestIncompleteGenIndex].Generation)) ||
         Stream->GenMeta[OldestIncompleteGenIndex].Complete)
      {
        OldestIncompleteGenIndex = GenerationIndex;
      }
    }
  }
  u8 OldestPreferablyIncompleteGenIndex = OldestIncompleteGenIndex;
  if(Stream->GenMeta[OldestIncompleteGenIndex].Complete)
  {
    OldestPreferablyIncompleteGenIndex = OldestGenIndex;
  }

  //NOTE(Bjorn): No good spot found. A generation has to go.
  Stream->GenMeta[OldestPreferablyIncompleteGenIndex].Generation = Generation;
  Stream->GenMeta[OldestPreferablyIncompleteGenIndex].Complete = false;
  Stream->GenMeta[OldestPreferablyIncompleteGenIndex].TotalDgramCount = 0;
  Stream->GenMeta[OldestPreferablyIncompleteGenIndex].FoundDgramCount = 1;
  *GetDatagram(Stream, DatagramIndex, OldestPreferablyIncompleteGenIndex) = *Datagram;

  if(TotalDgrams == 1)
  {
    Stream->GenMeta[OldestPreferablyIncompleteGenIndex].Complete = true;
    Stream->GenMeta[OldestPreferablyIncompleteGenIndex].TotalDgramCount = TotalDgrams;
  }
} 

internal_function datagram_stream*
CreateDatagramStream(memory_arena* Arena, u8 StreamDatagramLength, u8 ConcurrentGenerationCount)
{
  datagram_stream* Result = PushStruct(Arena, datagram_stream);

  Result->ConcurrentGenerationCount = ConcurrentGenerationCount;
  Result->StreamDatagramLength = StreamDatagramLength;

  Result->Generation = 0;

  s32 TotalDatagramCount = StreamDatagramLength * ConcurrentGenerationCount;
  Result->Datagrams = PushArray(Arena, TotalDatagramCount, datagram);
  for(u8 DatagramIndex = 0;
      DatagramIndex < Result->StreamDatagramLength;
      DatagramIndex++)
  {
    ClearDatagram(Result->Datagrams + DatagramIndex);
  }

  Result->GenMeta = PushArray(Arena, ConcurrentGenerationCount, gen_meta_data);
  for(u8 GenerationIndex = 0;
      GenerationIndex < Result->ConcurrentGenerationCount;
      GenerationIndex++)
  {
    Result->GenMeta[GenerationIndex] = {};
  }
}

inline u8
IncrementGeneration(u8 CurrentGen)
{
  u8 Result = CurrentGen + 1;
  //if(CurrentGen == 0) { Result = 1; }
  return Result;
}

internal_function void
PrepDatagramStreamForPacking(datagram_stream* Stream, u8 GenerationIndex = 0)
{
  Stream->Generation = IncrementGeneration(Stream->Generation);
  Stream->CurrentPayloadDatagramLenght = 0;

  for(u8 DatagramIndex = 0;
      DatagramIndex < Stream->StreamDatagramLength;
      DatagramIndex++)
  {
    datagram* Datagram = GetDatagram(Stream, DatagramIndex, GenerationIndex);
    ClearDatagram(Datagram);
  }
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
