#include "kvlAtlasMeshCollection.h"


int main( int argc, char** argv )
{
  // Sanity check on input
  if ( argc < 3 )
    {
    std::cerr << "Usage: "<< argv[ 0 ] << " meshName meshNumber [meshToCopyFrom]" << std::endl;
    return -1;
    }


  // Read mesh collection from file
  kvl::AtlasMeshCollection::Pointer  collection =  kvl::AtlasMeshCollection::New();
  collection->Read( argv[ 1 ] );

  // Retrieve the mesh number input
  std::istringstream  meshNumberStream( argv[ 2 ] );
  int  meshNumber;
  meshNumberStream >> meshNumber;

  // Read the mesh to copy from from file, if needed.
  kvl::AtlasMeshCollection::Pointer  collectionToCopyFrom;
  if ( argc > 3 )
    {
    collectionToCopyFrom =  kvl::AtlasMeshCollection::New();
    collectionToCopyFrom->Read( argv[ 3 ] );
    }
  else
    {
    collectionToCopyFrom = collection;
    } 

  // Set the reference position to the desired mesh position
  collection->SetReferencePosition( collectionToCopyFrom->GetPositions()[ meshNumber ] );

  // Write the result out
  std::ostringstream  outputFileNameStream;
  outputFileNameStream << argv[ 1] << "_referencePositionSet.txt";
  if ( !collection->Write( outputFileNameStream.str().c_str() ) )
    {
    std::cerr << "Couldn't not write mesh collection to file " << outputFileNameStream.str() << std::endl;
    }
  std::cout << "Just wrote mesh collection to file " << outputFileNameStream.str() << std::endl;


  return 0;
};


