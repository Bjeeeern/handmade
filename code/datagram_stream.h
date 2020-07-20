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
  b32 Discarded;
  u8 FoundDgramCount;
  u8 TotalDgramCount;
  u8 Generation;
};

struct datagram_stream_in
{
  u8 ConcurrentGenerationCount;
  u8 DatagramCount;

  datagram* Datagrams;
  gen_meta_data* GenMeta;
};
struct datagram_stream_out
{
  u8 DatagramCount;
  datagram* Datagrams;

  u8 DatagramUsedCount;
  u8 Generation;
};

inline datagram*
GetDatagram(datagram_stream_out* Stream, u8 DatagramIndex)
{
  Assert(DatagramIndex < Stream->DatagramCount);
  return Stream->Datagrams + DatagramIndex;
}
inline datagram*
GetDatagram(datagram_stream_in* Stream, u8 DatagramIndex, u8 GenerationIndex)
{
  Assert(DatagramIndex < Stream->DatagramCount);
  Assert(GenerationIndex <= Stream->ConcurrentGenerationCount);
  return Stream->Datagrams + ((GenerationIndex-1) * Stream->DatagramCount + DatagramIndex);
}
inline gen_meta_data*
GetGenMeta(datagram_stream_in* Stream, u8 GenerationIndex)
{
  Assert(GenerationIndex <= Stream->ConcurrentGenerationCount);
  return Stream->GenMeta + (GenerationIndex - 1);
}

inline s32
GenerationAgeDifference(u8 A, u8 B)
{
  s32 Diff = B - A;

  if(AbsoluteS32(Diff) < (max_u8 / 2))
  {
    return Diff;
  }
  else
  {
    return -Diff;
  }
}

internal_function void
AddDatagramToStream(datagram* Datagram, datagram_stream_in* Stream)
{
  u8 Generation = Datagram->Payload[0];
  u8 DatagramIndex = Datagram->Payload[1];
  u8 TotalDgrams = 0;
  if(DatagramIndex == 0)
  {
    TotalDgrams = Datagram->Payload[2];
  }

  gen_meta_data* Candidate = 0;
  u8 CandidateGenIndex = 0;
  for(u8 GenerationIndex = 1;
      GenerationIndex <= Stream->ConcurrentGenerationCount;
      GenerationIndex++)
  {
    gen_meta_data* Meta = GetGenMeta(Stream, GenerationIndex);
    s32 AgeDiff = GenerationAgeDifference(Generation, Meta->Generation);
    if(AgeDiff == 0)
    {
      Candidate = Meta;
      CandidateGenIndex = GenerationIndex;
      break;
    }
    else if(AgeDiff < 0)
    {
      continue;
    }
    else
    {
      if(!Candidate)
      {
        Candidate = Meta;
        CandidateGenIndex = GenerationIndex;
        continue;
      }

      //TODO double-check logic.
      if(!(!Meta->Discarded && Meta->Complete && !Candidate->Complete))
      {
        Candidate = Meta;
        CandidateGenIndex = GenerationIndex;
      }
    }
  }

  if(Candidate)
  {
    if(Candidate->Generation == Generation)
    {
      gen_meta_data* Meta = Candidate;
      Assert(!Meta->Complete);
      *GetDatagram(Stream, DatagramIndex, CandidateGenIndex) = *Datagram;
      Meta->FoundDgramCount += 1;

      if(DatagramIndex == 0)
      {
        Meta->TotalDgramCount = TotalDgrams;
      }
      if(Meta->FoundDgramCount == Meta->TotalDgramCount)
      {
        Meta->Complete = true;
      }
    }
    else
    {
      //NOTE(Bjorn): No good spot found. A generation has to go.
      gen_meta_data* Meta = Candidate;
      Meta->Generation = Generation;
      Meta->Complete = false;
      Meta->Discarded = false;
      Meta->TotalDgramCount = TotalDgrams;
      Meta->FoundDgramCount = 1;
      *GetDatagram(Stream, DatagramIndex, CandidateGenIndex) = *Datagram;

      if(Meta->FoundDgramCount == Meta->TotalDgramCount)
      {
        Meta->Complete = true;
      }
    }
  }
  else
  {
    //NOTE(bjorn): Throw away datagram.
  }
} 

internal_function u8  
GetIndexOfMostRecentCompleteGeneration(datagram_stream_in* Stream)
{
  u8 Result = 0;
  gen_meta_data* PrevMeta = 0;
  for(u8 GenerationIndex = 1;
      GenerationIndex <= Stream->ConcurrentGenerationCount;
      GenerationIndex++)
  {
    gen_meta_data* Meta = GetGenMeta(Stream, GenerationIndex);

    if(Meta->Complete &&
       !Meta->Discarded)
    {
      if(Result == 0 ||
         (PrevMeta && GenerationAgeDifference(PrevMeta->Generation, Meta->Generation)<0))
      {
        Result = GenerationIndex;
        PrevMeta = Meta;
      }
    }
  }
  return Result;
}

internal_function void
DiscardGenerations(datagram_stream_in* Stream)
{
  for(u8 GenerationIndex = 1;
      GenerationIndex <= Stream->ConcurrentGenerationCount;
      GenerationIndex++)
  {
    gen_meta_data* Meta = GetGenMeta(Stream, GenerationIndex);
    Meta->Discarded = true;
  }
}

internal_function datagram_stream_out*
CreateDatagramStreamOut(memory_arena* Arena, u8 StreamDatagramLength)
{
  datagram_stream_out* Result = PushStruct(Arena, datagram_stream_out);

  Result->DatagramCount = StreamDatagramLength;
  Result->Generation = 0;
  Result->Datagrams = PushArray(Arena, StreamDatagramLength, datagram);

  return Result;
}

internal_function datagram_stream_in*
CreateDatagramStreamIn(memory_arena* Arena, u8 StreamDatagramLength, u8 ConcurrentGenerationCount)
{
  datagram_stream_in* Result = PushStruct(Arena, datagram_stream_in);

  Result->ConcurrentGenerationCount = ConcurrentGenerationCount;
  Result->DatagramCount = StreamDatagramLength;

  s32 TotalDatagramCount = StreamDatagramLength * ConcurrentGenerationCount;
  Result->Datagrams = PushArray(Arena, TotalDatagramCount, datagram);

  Result->GenMeta = PushArray(Arena, ConcurrentGenerationCount, gen_meta_data);
  for(u8 GenerationIndex = 1;
      GenerationIndex <= ConcurrentGenerationCount;
      GenerationIndex++)
  {
    *GetGenMeta(Result, GenerationIndex) = {};
  }

  return Result;
}

inline u8
IncrementGeneration(u8 CurrentGen)
{
  u8 Result = CurrentGen == 255 ? 0 : CurrentGen + 1;
  //if(CurrentGen == 0) { Result = 1; }
  return Result;
}

internal_function void
PrepDatagramStreamForPacking(datagram_stream_out* Stream)
{
  Stream->Generation = IncrementGeneration(Stream->Generation);
  Stream->DatagramUsedCount = 0;

  for(u8 DatagramIndex = 0;
      DatagramIndex < Stream->DatagramCount;
      DatagramIndex++)
  {
    ClearDatagram(GetDatagram(Stream, DatagramIndex));
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
PackData_(datagram_stream_out* DatagramStream, u8* Data, memi Size)
{
  Assert(Size);

  memi SizeLeft = Size;
  for(u8 DatagramIndex = 0;
      DatagramIndex < DatagramStream->DatagramCount;
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

      DatagramStream->DatagramUsedCount += 1;
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
PackVar(datagram_stream_out* DatagramStream, type Var) \
{ \
  memi Size = sizeof(type); \
  type* Data = &Var; \
 \
  PackData_(DatagramStream, Data, Size); \
}

internal_function void
UnpackData_(datagram* Datagram, u8* Destination, memi Size)
{
  Assert(Datagram->UnpackOffset < Datagram->Used);
  Assert(Size <= Datagram->Used - Datagram->UnpackOffset);
  RtlCopyMemory(Destination, Datagram->Payload + Datagram->UnpackOffset, Size);
  Datagram->UnpackOffset += Size;
}

#define UnpackData(datagram_stream, gen_index, destination) UnpackData_((datagram_stream), (gen_index), (u8*)(destination), sizeof(*destination))
#define UnpackBuffer(datagram_stream, gen_index, destination, size) UnpackData_((datagram_stream), (gen_index), (destination), (size))
internal_function void
UnpackData_(datagram_stream_in* Stream, u8 GenerationIndex, u8* Destination, memi Size)
{
  Assert(GenerationIndex <= Stream->ConcurrentGenerationCount);
  gen_meta_data* Meta = GetGenMeta(Stream, GenerationIndex);
  Assert(Meta->Complete);
  Assert(Meta->FoundDgramCount == Meta->TotalDgramCount);

  memi SizeLeft = Size;
  memi SizeRead = 0;
  b32 DatagramWasUnpacked = false;
  for(u8 DatagramIndex = 0;
      DatagramIndex < Meta->TotalDgramCount;
      DatagramIndex++)
  {
    datagram* Dgram = GetDatagram(Stream, DatagramIndex, GenerationIndex);
    if(Dgram->UnpackOffset == Dgram->Used)
    {
      continue;
    }
    else
    {
      memi ReadableSize = Dgram->Used - Dgram->UnpackOffset;
      memi SizeToRead = Min(ReadableSize, SizeLeft);
      UnpackData_(Dgram, Destination + SizeRead, SizeToRead);
      SizeLeft -= SizeToRead;
      SizeRead += SizeToRead;
      if(SizeLeft == 0)
      {
        DatagramWasUnpacked = true;
        break;
      }
    }
  }
  Assert(DatagramWasUnpacked);
}
#define GenUnpackVarByType(type) \
internal_function type \
UnpackVar(datagram_stream_in* Stream, u8 GenerationIndex) \
{ \
  type Result; \
  memi Size = sizeof(type); \
 \
  UnpackData_(Stream, GenerationIndex, (u8*)&Result, Size); \
 \
  return Result; \
}

#define GenPackUnpackVarByType(type) GenPackVarByType(type) GenUnpackVarByType(type)
GenPackUnpackVarByType(u8);

#define DATAGRAM_STREAM
#endif
