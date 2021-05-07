#if !defined(ASCII_PARSE_H)

internal_function u32
ParseUnsignedInteger(u8* Start)
{
  u8* End = Start;
  while('0' <= *End && *End <= '9') { End++; }

  u32 f = 1;
  u32 n = 0;

  u8* Cursor = End-1;
  while(Cursor != (Start-1))
  {
    u32 d = Cursor[0] - '0';
    n += f*d;
    f *= 10;

    Cursor -= 1;
  }

  return n;
}

internal_function f32
ParseDecimal(u8* Start)
{
  f64 sign = 1.0;
  if(Start[0] == '-')
  {
    sign = -1.0;
    Start += 1;
  }

  u8* Dot = Start;
  while('0' <= *Dot && *Dot <= '9') { Dot++; }
  u8* End = Dot+1;
  while('0' <= *End && *End <= '9') { End++; }

  f64 whole = ParseUnsignedInteger(Start);

  // TODO(bjorn): Implement Pow()?
  s32 exponent = (s32)((End-1) - Dot);
  f64 f = 1.0;
  for(s32 Times = 0;
      Times < exponent;
      Times += 1)
  {
    f *= 0.1;
  }

  f64 part = ParseUnsignedInteger(Dot+1) * f;

  return (f32)(sign * (whole + part));
}

#define ASCII_PARSE_H
#endif
