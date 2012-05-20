package com.paulasmuth.fyrehose

import com.google.gson._
import java.io._

object EventParser{
  

  def parse(raw: Array[Byte]) : JsonObject = try{
    parse_unsafe(raw)
  } catch {
    case e: com.google.gson.JsonParseException => 
      throw new ParseException("invalid json")
  }


  private def parse_unsafe(raw: Array[Byte]) : JsonObject = {
    (new JsonParser()).parse(
      new InputStreamReader(
        new ByteArrayInputStream(raw)))
    match {
      case obj: JsonObject => return obj
      case _ => throw new ParseException("not a json-object")
    }
  }


}  