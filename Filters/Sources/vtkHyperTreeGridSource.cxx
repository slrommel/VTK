/*=========================================================================

Program:   Visualization Toolkit
Module:    vtkHyperTreeGridSource.cxx

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkHyperTreeGridSource.h"

#include "vtkBitArray.h"
#include "vtkDataArray.h"
#include "vtkDoubleArray.h"
#include "vtkHyperTreeGrid.h"
#include "vtkHyperTreeCursor.h"
#include "vtkInformationVector.h"
#include "vtkInformation.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"

#include <vtksys/ios/sstream>

#include <assert.h>

vtkStandardNewMacro(vtkHyperTreeGridSource);

//----------------------------------------------------------------------------
vtkHyperTreeGridSource::vtkHyperTreeGridSource()
{
  // This a source: no input ports
  this->SetNumberOfInputPorts( 0 );

  // Grid parameters
  this->BranchFactor = 2;
  this->MaximumLevel = 1;

  // Grid topology
  this->Dimension = 3;
  this->GridSize[0] = 1;
  this->GridSize[1] = 1;
  this->GridSize[2] = 1;

  // Grid geometry
  this->GridScale[0] = 1.;
  this->GridScale[1] = 1.;
  this->GridScale[2] = 1.;
  this->XCoordinates = vtkDoubleArray::New();
  this->XCoordinates->SetNumberOfTuples( 2 );
  this->XCoordinates->SetComponent( 0, 0, 0. );
  this->XCoordinates->SetComponent( 1, 0, this->GridScale[0] );
  this->YCoordinates = vtkDoubleArray::New();
  this->YCoordinates->SetNumberOfTuples( 2 );
  this->YCoordinates->SetComponent( 0, 0, 0. );
  this->YCoordinates->SetComponent( 1, 0, this->GridScale[1] );
  this->ZCoordinates = vtkDoubleArray::New();
  this->ZCoordinates->SetNumberOfTuples( 2 );
  this->ZCoordinates->SetComponent( 0, 0, 0. );
  this->ZCoordinates->SetComponent( 1, 0, this->GridScale[2] );

  // By default expose the primal grid API
  this->Dual = false;

  // By default do not use the material mask
  this->UseMaterialMask = false;

  // Grid description
  this->Descriptor = ".";

  // Material mask
  this->MaterialMask = "0";

  this->Output = NULL;
}

//----------------------------------------------------------------------------
vtkHyperTreeGridSource::~vtkHyperTreeGridSource()
{
  if ( this->XCoordinates )
    {
    this->XCoordinates->UnRegister( this );
    this->XCoordinates = NULL;
    }

  if ( this->YCoordinates )
    {
    this->YCoordinates->UnRegister( this );
    this->YCoordinates = NULL;
    }

  if ( this->ZCoordinates )
    {
    this->ZCoordinates->UnRegister( this );
    this->ZCoordinates = NULL;
    }
}

//-----------------------------------------------------------------------------
void vtkHyperTreeGridSource::PrintSelf( ostream& os, vtkIndent indent )
{
  this->Superclass::PrintSelf( os, indent );

  os << indent << "GridSize: "
     << this->GridSize[0] <<","
     << this->GridSize[1] <<","
     << this->GridSize[2] << endl;

  os << indent << "GridScale: "
     << this->GridScale[0] <<","
     << this->GridScale[1] <<","
     << this->GridScale[2] << endl;

  os << indent << "MaximumLevel: " << this->MaximumLevel << endl;
  os << indent << "Dimension: " << this->Dimension << endl;
  os << indent << "BranchFactor: " <<this->BranchFactor << endl;
  os << indent << "BlockSize: " <<this->BlockSize << endl;

  if ( this->XCoordinates )
    {
    this->XCoordinates->PrintSelf( os, indent.GetNextIndent() );
    }
  if ( this->YCoordinates )
    {
    this->YCoordinates->PrintSelf( os, indent.GetNextIndent() );
    }
  if ( this->ZCoordinates )
    {
    this->ZCoordinates->PrintSelf( os, indent.GetNextIndent() );
    }

  os << indent << "Dual: " << this->Dual << endl;
  os << indent << "UseMaterialMask: " << this->UseMaterialMask << endl;

  os << indent << "Descriptor: " << this->Descriptor << endl;
  os << indent << "MaterialMask: " << this->Descriptor << endl;
  os << indent << "LevelDescriptors: " << this->LevelDescriptors.size() << endl;
  os << indent << "LevelMaterialMasks: " << this->LevelMaterialMasks.size() << endl;
  os << indent << "LevelCounters: " << this->LevelCounters.size() << endl;

  os << indent
     << "Output: ";
  if ( this->Output )
    {
    this->Output->PrintSelf( os, indent );
    }
  else
    {
    os << "(none)" << endl;
    }
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridSource::SetDescriptor( const vtkStdString& string )
{
  this->Descriptor = string;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkStdString vtkHyperTreeGridSource::GetDescriptor()
{
  return this->Descriptor;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridSource::SetMaterialMask( const vtkStdString& string )
{
  this->MaterialMask = string;
  this->Modified();
}

//----------------------------------------------------------------------------
vtkStdString vtkHyperTreeGridSource::GetMaterialMask()
{
  return this->MaterialMask;
}

//----------------------------------------------------------------------------
// Description:
// Return the maximum number of levels of the hyperoctree.
// \post positive_result: result>=1
unsigned int vtkHyperTreeGridSource::GetMaximumLevel()
{
  assert( "post: positive_result" && this->MaximumLevel >= 1 );
  return this->MaximumLevel;
}

//----------------------------------------------------------------------------
// Description:
// Set the maximum number of levels of the hypertrees. If
// GetMinLevels()>=levels, GetMinLevels() is changed to levels-1.
// \pre positive_levels: levels>=1
// \post is_set: this->GetLevels()==levels
// \post min_is_valid: this->GetMinLevels()<this->GetLevels()
void vtkHyperTreeGridSource::SetMaximumLevel( unsigned int levels )
{
  if ( levels < 1 )
    {
    levels = 1;
    }

  if ( this->MaximumLevel == levels )
    {
    return;
    }

  this->MaximumLevel = levels;
  this->Modified();

  assert( "post: is_set" && this->GetMaximumLevel() == levels );
}


//----------------------------------------------------------------------------
int vtkHyperTreeGridSource::RequestInformation( vtkInformation*,
                                                vtkInformationVector**,
                                                vtkInformationVector* outputVector )
{
  // get the info objects
  vtkInformation* outInfo = outputVector->GetInformationObject(0);

  // We cannot give the exact number of levels of the hypertrees
  // because it is not generated yet and this process depends on the recursion formula.
  // Just send an upper limit instead.
  outInfo->Set( vtkHyperTreeGrid::LEVELS(), this->MaximumLevel );
  outInfo->Set( vtkHyperTreeGrid::DIMENSION(), this->Dimension );

  double origin[3];
  origin[0] = this->XCoordinates->GetTuple1( 0 );
  origin[1] = this->YCoordinates->GetTuple1( 0 );
  origin[2] = this->ZCoordinates->GetTuple1( 0 );
  outInfo->Set( vtkDataObject::ORIGIN(), origin, 3 );

  return 1;
}

//----------------------------------------------------------------------------
int vtkHyperTreeGridSource::RequestData( vtkInformation*,
                                         vtkInformationVector**,
                                         vtkInformationVector* outputVector )
{
  // Retrieve the output
  vtkInformation *outInfo = outputVector->GetInformationObject( 0 );
  this->Output = vtkHyperTreeGrid::SafeDownCast( outInfo->Get(vtkDataObject::DATA_OBJECT()) );
  if ( ! this->Output )
    {
    return 0;
    }

  // Initialize descriptor parsing
  if ( ! this->Initialize() )
    {
    return 0;
    }

  // Set grid parameters
  this->Output->SetGridSize( this->GridSize );
  this->Output->SetDimension( this->Dimension );
  this->Output->SetBranchFactor( this->BranchFactor );
  this->Output->SetUseDualGrid( this->Dual );

  // Create geometry
  for ( int i = 0; i < 3; ++ i )
    {
    vtkDoubleArray *coords = vtkDoubleArray::New();
    int n = this->GridSize[i] + 1;
    coords->SetNumberOfValues( n );
    for ( int j = 0; j < n; ++ j )
      {
      coords->SetValue( j, this->GridScale[i] * static_cast<double>( j ) );
      }

    switch ( i )
      {
      case 0:
        this->Output->SetXCoordinates( coords );
        break;
      case 1:
        this->Output->SetYCoordinates( coords );
        break;
      case 2:
        this->Output->SetZCoordinates( coords );
        break;
      }

    // Clean up
    coords->Delete();
    }

  // Prepare array of doubles for cell values
  vtkDoubleArray* scalars = vtkDoubleArray::New();
  scalars->SetName( "Cell Value" );
  scalars->SetNumberOfComponents( 1 );
  vtkIdType fact = 1;
  for ( unsigned int i = 1; i < this->MaximumLevel; ++ i )
    {
    fact *= this->BranchFactor;
    }
  scalars->Allocate( fact * fact );

  // Set leaf (cell) data and clean up
  this->Output->GetLeafData()->SetScalars( scalars );
  scalars->UnRegister( this );

  // Iterate over grid of trees
  unsigned int n[3];
  this->Output->GetGridSize( n );
  for ( unsigned int k = 0; k < n[2]; ++ k )
    {
    for ( unsigned int j = 0; j < n[1]; ++ j )
      {
      for ( unsigned int i = 0; i < n[0]; ++ i )
        {
        // Calculate tree index
        int treeIdx = ( k * this->GridSize[1] + j ) * this->GridSize[0] + i;

        // Initialize cursor
        vtkHyperTreeCursor* cursor = this->Output->NewCursor( treeIdx );
        cursor->ToRoot();

        // Initialize local cell index
        int idx[3];
        idx[0] = idx[1] = idx[2] = 0;

        // Retrieve offset into array of scalars and recurse
        this->Subdivide( cursor,
                         0,
                         treeIdx,
                         0,
                         idx,
                         this->Output->GetLeafData()->GetScalars()->GetNumberOfTuples(),
                         0 );

        // Clean up
        cursor->UnRegister( this );
        } // i
      } // j
    } // k

  assert( "post: dataset_and_data_size_match" && this->Output->CheckAttributes() == 0 );

  return 1;
}

//-----------------------------------------------------------------------------
int vtkHyperTreeGridSource::Initialize()
{
  // Verify that grid and material specifications are consistent
  if (  this->UseMaterialMask
        && this->MaterialMask.size() != this->Descriptor.size() )
    {
    vtkErrorMacro(<<"Material mask is used but has length "
                  << this->MaterialMask.size()
                  << " != "
                  << this->Descriptor.size()
                  << " which is the length of the grid descriptor.");

    return 0;
    }

   // Calculate refined block size
  this->BlockSize = this->BranchFactor;
  for ( unsigned int i = 1; i < this->Dimension; ++ i )
    {
    this->BlockSize *= this->BranchFactor;
    }

  // Calculate total level 0 grid size
  unsigned int nTotal = this->GridSize[0] * this->GridSize[1] * this->GridSize[2];

  // Initialize material mask iterator only if needed
  vtkStdString::iterator mit;
  if ( this->UseMaterialMask )
    {
    mit = this->MaterialMask.begin();
    }

  // Parse string descriptor and material mask if used
  unsigned int nRefined = 0;
  unsigned int nLeaves = 0;
  unsigned int nNextLevel = nTotal;
  bool rootLevel = true;
  vtksys_ios::ostringstream descriptor;
  vtksys_ios::ostringstream mask;
  for ( vtkStdString::iterator dit = this->Descriptor.begin(); dit != this->Descriptor.end();  ++ dit )
    {
    switch ( *dit )
      {
      case ' ':
        // Space is allowed as separator, verify mask consistenty if needed
        if ( this->UseMaterialMask && *mit != ' ' )
          {
          vtkErrorMacro(<<"Space separators do not match between descriptor and material mask.");
          return 0;
          }

        // Advance material mask iterator only if needed
        if ( this->UseMaterialMask )
          {
          ++ mit;
          }

        continue; // case ' '
      case '|':
        //  A level is complete, verify mask consistenty if needed
        if ( this->UseMaterialMask && *mit != '|' )
          {
          vtkErrorMacro(<<"Level separators do not match between descriptor and material mask.");
          return 0;
          }

        // Store descriptor and material mask for current level
        this->LevelDescriptors.push_back( descriptor.str().c_str() );
        this->LevelMaterialMasks.push_back( mask.str().c_str() );

        // Check whether cursor is still at rool level
        if ( rootLevel )
          {
          rootLevel = false;

          // Verify that total number of root cells is consistent with descriptor
          if ( nRefined + nLeaves != nTotal )
            {
            vtkErrorMacro(<<"String "
                          << this->Descriptor
                          << " describes "
                          << nRefined + nLeaves
                          << " root cells != "
                          << nTotal);
            return 0;
            }
          } // if ( rootLevel )
        else
          {
          // Verify that level descriptor cardinality matches expected value
          if (  descriptor.str().size() != nNextLevel )
            {
            vtkErrorMacro(<<"String level descriptor "
                          << descriptor.str().c_str()
                          << " has cardinality "
                          << descriptor.str().size()
                          << " which is not expected value of "
                          << nNextLevel);

            return 0;
            }
          } // else

        // Predict next level descriptor cardinality
        nNextLevel = nRefined * this->BlockSize;

        // Reset per level values
        descriptor.str( "" );
        mask.str( "" );
        nRefined = 0;
        nLeaves = 0;

        break; // case '|'
      case 'R':
        //  Refined cell, verify mask consistenty if needed
        if ( this->UseMaterialMask && *mit == '0' )
          {
          vtkErrorMacro(<<"A refined branch must contain material.");
          return 0;
          }
        // Refined cell, update branch counter
        ++ nRefined;

        // Append characters to per level descriptor and material mask if used
        descriptor << *dit;
        if ( this->UseMaterialMask )
          {
          mask << *mit;
          }

        break; // case 'R'
      case '.':
        // Leaf cell, update leaf counter
        ++ nLeaves;

        // Append characters to per level descriptor and material mask if used
        descriptor << *dit;
        if ( this->UseMaterialMask )
          {
          mask << *mit;
          }

        break; // case '.'
      default:
        vtkErrorMacro(<< "Unrecognized character: "
                      << *dit
                      << " in string "
                      << this->Descriptor);

        return 0; // default
      } // switch( *dit )

    // Advance material mask iterator only if needed
    if ( this->UseMaterialMask )
      {
      ++ mit;
      }
    } // dit

  // Verify and append last level string
  if (  descriptor.str().size() != nNextLevel )
    {
    vtkErrorMacro(<<"String level descriptor "
                  << descriptor.str().c_str()
                  << " has cardinality "
                  << descriptor.str().size()
                  << " which is not expected value of "
                  << nNextLevel);

    return 0;
    }

  // Push per-level descriptor and material mask if used
  this->LevelDescriptors.push_back( descriptor.str().c_str() );
  if ( this->UseMaterialMask )
    {
    this->LevelMaterialMasks.push_back( mask.str().c_str() );
    }

  // Reset maximum depth if fewer levels are described
  unsigned int nLevels = static_cast<unsigned int>( this->LevelDescriptors.size() );
  if ( nLevels < this->MaximumLevel )
    {
    this->MaximumLevel = nLevels;
    }

  // Create vector of counters as long as tree depth
  for ( unsigned int i = 0; i < nLevels; ++ i )
    {
    this->LevelCounters.push_back( 0 );
    }

  return 1;
}

//----------------------------------------------------------------------------
void vtkHyperTreeGridSource::Subdivide( vtkHyperTreeCursor* cursor,
                                        unsigned int level,
                                        int treeIdx,
                                        int childIdx,
                                        int idx[3],
                                        int cellIdOffset,
                                        int parentPos )
{
  // Calculate pointer into level descriptor string
  int pointer = level ? childIdx + parentPos * this->BlockSize : treeIdx;

  // Determine whether to subdivide or not
  bool subdivide = this->LevelDescriptors.at( level ).at( pointer ) == 'R' ? true : false;

  // Check for hard coded minimum and maximum level restrictions
  if ( level + 1 >= this->MaximumLevel )
    {
    subdivide = 0;
    }

  if ( subdivide )
    {
    // Subdivide hyper tree grid leaf
    this->Output->SubdivideLeaf( cursor, treeIdx );

    // Now traverse to children.
    int xDim = 1;
    int yDim = 1;
    int zDim = 1;
    switch ( this->Dimension )
      {
      // Warning: Run through is intended! Do NOT add break statements
      case 3:
        zDim = this->BranchFactor;
      case 2:
        yDim = this->BranchFactor;
      case 1:
        xDim = this->BranchFactor;
      }

    int newChildIdx = 0;
    int newIdx[3];
    for ( int z = 0; z < zDim; ++ z )
      {
      newIdx[2] = idx[2] * zDim + z;
      for ( int y = 0; y < yDim; ++ y )
        {
        newIdx[1] = idx[1] * yDim + y;
        for ( int x = 0; x < xDim; ++ x )
          {
          newIdx[0] = idx[0] * xDim + x;

          // Set cursor to child
          cursor->ToChild( newChildIdx );

          // Recurse
          this->Subdivide( cursor,
                           level + 1,
                           treeIdx,
                           newChildIdx,
                           newIdx,
                           cellIdOffset,
                           this->LevelCounters.at( level ) );

          // Reset cursor to parent
          cursor->ToParent();

          // Increment child index
          ++ newChildIdx;
          }
        }
      }

    // Increment current level counter
    ++ this->LevelCounters.at( level );
    } // if ( subdivide )
  else 
    {
    // We are at a leaf cell, calculate its global index
    vtkIdType id = cellIdOffset + cursor->GetLeafId();

    // Blank leaf if needed
    if ( this->UseMaterialMask
         && this->LevelMaterialMasks.at( level ).at( pointer ) == '0' )
      {
      // Blank leaf in underlying hyper tree
      this->Output->GetMaterialMask()->InsertTuple1( id, 1 );
      }
    else
      {
      // Do not blank leaf in underlying hyper tree
      this->Output->GetMaterialMask()->InsertTuple1( id, 0 );
      }

    // Cell value is depth level for now
    this->Output->GetLeafData()->GetScalars()->InsertTuple1( id, level );
    } // else
}

//-----------------------------------------------------------------------------