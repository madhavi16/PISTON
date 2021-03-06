/*
Copyright (c) 2011, Los Alamos National Security, LLC
All rights reserved.
Copyright 2011. Los Alamos National Security, LLC. This software was produced under U.S. Government contract DE-AC52-06NA25396 for Los Alamos National Laboratory (LANL),
which is operated by Los Alamos National Security, LLC for the U.S. Department of Energy. The U.S. Government has rights to use, reproduce, and distribute this software.

NEITHER THE GOVERNMENT NOR LOS ALAMOS NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.

If software is modified to produce derivative works, such modified software should be clearly marked, so as not to confuse it with the version available from LANL.

Additionally, redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
·         Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
·         Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other
          materials provided with the distribution.
·         Neither the name of Los Alamos National Security, LLC, Los Alamos National Laboratory, LANL, the U.S. Government, nor the names of its contributors may be used
          to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY LOS ALAMOS NATIONAL SECURITY, LLC AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LOS ALAMOS NATIONAL SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef GLYPH_H_
#define GLYPH_H_

#include <thrust/copy.h>
#include <thrust/scan.h>
#include <thrust/transform_scan.h>
#include <thrust/binary_search.h>
#include <thrust/iterator/constant_iterator.h>
#include <thrust/iterator/zip_iterator.h>

#include <piston/image3d.h>
#include <piston/piston_math.h>
#include <piston/choose_container.h>


namespace piston {

template <typename InputPoints, typename InputVectors, typename InputScalars, typename GlyphVertices, typename GlyphNormals, typename GlyphIndices>
class glyph
{
public:
	InputPoints inputPoints;
	InputVectors inputVectors;
	InputScalars inputScalars;
	GlyphVertices glyphVertices;
	GlyphNormals glyphNormals;
	GlyphIndices glyphIndices;

	int nPoints, nVertices, nIndices;

	typedef typename detail::choose_container<GlyphVertices, float3>::type VerticesContainer;
	typedef typename detail::choose_container<GlyphNormals, float3>::type	NormalsContainer;
	typedef typename detail::choose_container<InputScalars, float>::type	ScalarsContainer;
	typedef typename detail::choose_container<GlyphIndices, uint3>::type	IndicesContainer;

	typedef typename VerticesContainer::iterator VerticesIterator;
	typedef typename IndicesContainer::iterator  IndicesIterator;
	typedef typename NormalsContainer::iterator  NormalsIterator;
	typedef typename ScalarsContainer::iterator  ScalarsIterator;

	typedef typename thrust::counting_iterator<int>	CountingIterator;

	VerticesContainer	vertices;
	NormalsContainer	normals;
	IndicesContainer	indices;
    ScalarsContainer    scalars;

    unsigned int numIndices;
    float minValue, maxValue;
    bool useInterop;
    float3 *vertexBufferData;
    float3 *normalBufferData;
    float4 *colorBufferData;
    uint3 *indexBufferData;
#ifdef USE_INTEROP
    struct cudaGraphicsResource* vboResources[4];
#endif

	glyph(InputPoints inputPoints, InputVectors inputVectors, InputScalars inputScalars, GlyphVertices glyphVertices, GlyphNormals glyphNormals, GlyphIndices glyphIndices,
		  int nPoints, int nVertices, int nIndices) : inputPoints(inputPoints), inputVectors(inputVectors), inputScalars(inputScalars), glyphVertices(glyphVertices), glyphNormals(glyphNormals),
		  glyphIndices(glyphIndices), nPoints(nPoints), nVertices(nVertices), nIndices(nIndices), useInterop(false) {};

	void operator()()
	{
	  int NCells = nPoints;
	  normals.resize(NCells*nVertices);
	  indices.resize(NCells*nIndices);
	  vertices.resize(NCells*nVertices);
	  scalars.resize(NCells*nVertices);
	  numIndices = NCells*nIndices;

#ifdef USE_INTEROP
	  if (useInterop)
	  {
	    size_t num_bytes;
	    cudaGraphicsMapResources(1, &vboResources[0], 0);
	    cudaGraphicsResourceGetMappedPointer((void **)&vertexBufferData, &num_bytes, vboResources[0]);

	    if (vboResources[1])
	    {
	      cudaGraphicsMapResources(1, &vboResources[1], 0);
	      cudaGraphicsResourceGetMappedPointer((void **)&colorBufferData, &num_bytes, vboResources[1]);
	    }

	    cudaGraphicsMapResources(1, &vboResources[2], 0);
	    cudaGraphicsResourceGetMappedPointer((void **)&normalBufferData, &num_bytes, vboResources[2]);

	    cudaGraphicsMapResources(1, &vboResources[3], 0);
	    cudaGraphicsResourceGetMappedPointer((void **)&indexBufferData, &num_bytes, vboResources[3]);
	  }

      if (useInterop)
      {
    	thrust::for_each(CountingIterator(0), CountingIterator(0)+NCells, generate_glyphs(inputPoints, inputVectors, inputScalars, glyphVertices, glyphNormals, glyphIndices,
    	  			     vertexBufferData, normalBufferData, indexBufferData, thrust::raw_pointer_cast(&*scalars.begin()), nVertices, nIndices));
	    /*thrust::for_each(thrust::make_zip_iterator(thrust::make_tuple(validCellIndices.begin(), numVerticesEnum.begin(),
		                                           thrust::make_permutation_iterator(cubeIndex.begin(), validCellIndices.begin()),
		                                           thrust::make_permutation_iterator(numVertices.begin(), validCellIndices.begin()))),
		             thrust::make_zip_iterator(thrust::make_tuple(validCellIndices.end(), numVerticesEnum.end(),
		                                       thrust::make_permutation_iterator(cubeIndex.begin(), validCellIndices.begin()) + numValidCells,
		                                       thrust::make_permutation_iterator(numVertices.begin(), validCellIndices.begin()) + numValidCells)),
		             isosurface_functor(isovalue, input, source, xDim, yDim, CellsPerLayer,
		                                thrust::raw_pointer_cast(&*triTable.begin()),
		                                vertexBufferData, normalBufferData, thrust::raw_pointer_cast(&*scalars.begin())));*/
        if (vboResources[1]) thrust::transform(scalars.begin(), scalars.end(), thrust::device_ptr<float4>(colorBufferData), color_map<float>(minValue, maxValue));
      }
      else
#endif
      {
	    thrust::for_each(CountingIterator(0), CountingIterator(0)+NCells, generate_glyphs(inputPoints, inputVectors, inputScalars, glyphVertices, glyphNormals, glyphIndices,
			    thrust::raw_pointer_cast(&*vertices.begin()), thrust::raw_pointer_cast(&*normals.begin()), thrust::raw_pointer_cast(&*indices.begin()),
			    thrust::raw_pointer_cast(&*scalars.begin()), nVertices, nIndices));
      }

#ifdef USE_INTEROP
	  if (useInterop)
	  {
	    /*thrust::copy(vertices.begin(), vertices_end(), thrust::device_ptr<float3>)
	  	thrust::copy(thrust::make_transform_iterator(vertices_begin(), tuple2float4()),
	  	             thrust::make_transform_iterator(vertices_end(),   tuple2float4()),
	  	             thrust::device_ptr<float4>(vertexBufferData));
	  	thrust::copy(normals_begin(), normals_end(), thrust::device_ptr<float3>(normalBufferData));
	  	thrust::transform(scalars_begin(), scalars_end(),
	  		              thrust::device_ptr<float4>(colorBufferData),
	  		              color_map<float>(minThresholdRange, maxThresholdRange, true) );*/

	  	for (int i=0; i<4; i++) cudaGraphicsUnmapResources(1, &vboResources[i], 0);
	  }
#endif
	}

	struct generate_glyphs : public thrust::unary_function<int, void>
	{
	  InputPoints inputPoints;
	  InputVectors inputVectors;
	  InputScalars inputScalars;
	  GlyphVertices glyphVertices;
	  GlyphNormals glyphNormals;
	  GlyphIndices glyphIndices;

	  float3* vertices;
	  float3* normals;
	  uint3* indices;
	  float* scalars;

	  int nVertices, nIndices;

	  __host__ __device__
	  generate_glyphs(InputPoints inputPoints, InputVectors inputVectors, InputScalars inputScalars, GlyphVertices glyphVertices, GlyphNormals glyphNormals, GlyphIndices glyphIndices,
			          float3* vertices, float3* normals, uint3* indices, float* scalars, int nVertices, int nIndices) : inputPoints(inputPoints), inputVectors(inputVectors), inputScalars(inputScalars),
			          glyphVertices(glyphVertices), glyphNormals(glyphNormals), glyphIndices(glyphIndices), vertices(vertices), normals(normals), indices(indices), scalars(scalars),
			          nVertices(nVertices), nIndices(nIndices) {};

	  __host__ __device__
	  void operator() (int id) const
	  {
		float R[9]; //for (int i=0; i<9; i++) R[i] = 0.0;  R[1] = -1.0;  R[3] = 1.0;  R[8] = 1.0;  //R[0] = R[4] = R[8] = 1.0;

		float3 vector = *(inputVectors+id);
		R[0] = vector.x;  R[3] = vector.y;  R[6] = vector.z;
		if ((fabs(vector.x) > 0.00001) || (fabs(vector.y) > 0.00001)) { R[1] = vector.y;  R[4] = -vector.x; R[7] = 0.0; }
		else { R[1] = 0.0;  R[4] = 1.0;  R[7] = 0.0; }

		float norm = sqrt(R[0]*R[0]+R[3]*R[3]+R[6]*R[6]);
		R[0] /= norm;  R[3] /= norm;  R[6] /= norm;

		norm = sqrt(R[1]*R[1]+R[4]*R[4]+R[7]*R[7]);
		R[1] /= norm;  R[4] /= norm;  R[7] /= norm;

		R[2] = R[3]*R[7] - R[4]*R[6];
		R[5] = R[1]*R[6] - R[0]*R[7];
		R[8] = R[0]*R[4] - R[1]*R[3];

		norm = sqrt(R[2]*R[2]+R[5]*R[5]+R[8]*R[8]);
		R[2] /= norm;  R[5] /= norm;  R[8] /= norm;

		float3 base = *(inputPoints+id);
		float p[3]; p[0] = base.x; p[1] = base.y; p[2] = base.z;
		for (int i=id*(nVertices); i<(id+1)*(nVertices); i++)
		{
		  float3 result;
		  float3 glyphV = *(glyphVertices+(i%(nVertices)));
		  float x[3]; x[0] = glyphV.x; x[1] = glyphV.y; x[2] = glyphV.z;
		  result.x = *(inputScalars+id)*(R[0]*x[0] + R[1]*x[1] + R[2]*x[2]) + p[0];
		  result.y = *(inputScalars+id)*(R[3]*x[0] + R[4]*x[1] + R[5]*x[2]) + p[1];
		  result.z = *(inputScalars+id)*(R[6]*x[0] + R[7]*x[1] + R[8]*x[2]) + p[2];
		  *(vertices+i) = result;

		  float3 glyphN = *(glyphNormals+(i%(nVertices)));
		  x[0] = glyphN.x; x[1] = glyphN.y; x[2] = glyphN.z;
		  result.x = R[0]*x[0] + R[1]*x[1] + R[2]*x[2];
		  result.y = R[3]*x[0] + R[4]*x[1] + R[5]*x[2];
		  result.z = R[6]*x[0] + R[7]*x[1] + R[8]*x[2];
		  *(normals+i) = result;

		  *(scalars+i) = *(inputScalars+id);
		}

        //for (int i=id*(nVertices); i<(id+1)*(nVertices); i++) *(vertices+i) = *(glyphVertices+(i%(nVertices))) + *(inputPoints+id);
        //for (int i=id*(nVertices); i<(id+1)*(nVertices); i++) *(normals+i) = *(glyphNormals+(i%(nVertices)));

        uint3 offset; offset.x = offset.y = offset.z = id*nVertices;
        for (int i=id*nIndices; i<(id+1)*nIndices; i++)  *(indices+i) = (uint3)(*(glyphIndices+(i%nIndices))) + offset;
        //{
            //indices[i].x = glyphIndices[i%nIndices].x + offset.x;
        //}
	  }
	};

	VerticesIterator vertices_begin()  { return vertices.begin(); }
	VerticesIterator vertices_end()    { return vertices.end();   }
	NormalsIterator normals_begin()    { return normals.begin();  }
	NormalsIterator normals_end()      { return normals.end();    }
	IndicesIterator indices_begin()    { return indices.begin();  }
	IndicesIterator indices_end()      { return indices.end();    }
	ScalarsIterator scalars_begin()    { return scalars.begin(); }
	ScalarsIterator scalars_end()      { return scalars.end();   }
};

}

#endif /* GLYPH_H_ */
