# TextQueries
WordQuery, NotQuery, AndQuery, OrQuery
Read a file to build a word-lines map which denotes which word apears on which line
Four ways to query: 
  1) which line a word appears
  2) which line a word not appears
  3) which line multiple words both appear
  4) which line one of multiple words appear

Interface Classes
TextQuery
  Read a file and build a lookup map.
QueryResult
  Hold the results.
Query
  Point to an object of type derived from Query_base.
  
Implementation Classes
Query_base
  Abstract base class.
WordQuery
  Class derived from Query_base that looks for a given word.
NotQuery
  Class derived from Query_base that represents the set of lines in which its Query oprand does not appear.
BinaryQuery
  Abstract base class derived from Query_base that represents queries with two Query operand.
OrQuery
  Class derived from BinaryQuery that returns the union of the line numbers in which its two operands appear.
AndQuery
  Class derived from BinaryQuery that returns the intersection of the line numbers in which its two operands appear.
