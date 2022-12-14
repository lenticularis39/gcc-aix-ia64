/* Copyright (C) 2000  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

package java.awt.image;

import java.awt.Point;
import java.awt.Transparency;
import java.awt.color.ColorSpace;
import gnu.gcj.awt.Buffers;

/**
 * @author Rolf W. Rasmussen <rolfwr@ii.uib.no>
 */
public class DirectColorModel extends PackedColorModel
{
  public DirectColorModel(int pixelBits, int rmask, int gmask, int bmask)
  {
    this(ColorSpace.getInstance(ColorSpace.CS_sRGB), pixelBits,
	 rmask, gmask, bmask, 0, 
	 false, // not alpha premultiplied
	 Buffers.smallestAppropriateTransferType(pixelBits) // find type
	 );
  }

  public DirectColorModel(int pixelBits,
			  int rmask, int gmask, int bmask, int amask)
  {
    this(ColorSpace.getInstance(ColorSpace.CS_sRGB), pixelBits,
	 rmask, gmask, bmask, amask,
	 false, // not alpha premultiplied
	 Buffers.smallestAppropriateTransferType(pixelBits) // find type
	 );
  }

  public DirectColorModel(ColorSpace cspace, int pixelBits,
			  int rmask, int gmask, int bmask, int amask,
			  boolean isAlphaPremultiplied,
			  int transferType)
  {
    super(cspace, pixelBits,
	  rmask, gmask, bmask, amask, isAlphaPremultiplied,
	  ((amask == 0) ? Transparency.OPAQUE : Transparency.TRANSLUCENT),
	  transferType);
  }
    
  public final int getRedMask()
  {
    return getMask(0);
  }

  public final int getGreenMask()
  {
    return getMask(1);
  }

  public final int getBlueMask()
  {
    return getMask(2);
  }

  public final int getAlphaMask()
  {
    return hasAlpha() ? getMask(3) : 0;
  }

  public final int getRed(int pixel)
  {
    return extractAndNormalizeSample(pixel, 0);
  }

  public final int getGreen(int pixel)
  {
    return extractAndNormalizeSample(pixel, 1);
  }
  
  public final int getBlue(int pixel)
  {
    return extractAndNormalizeSample(pixel, 2);
  }

  public final int getAlpha(int pixel)
  {
    if (!hasAlpha()) return 0;
    return extractAndScaleSample(pixel, 3);
  }

  private final int extractAndNormalizeSample(int pixel, int component)
  {
    int value = extractAndScaleSample(pixel, component);
    if (hasAlpha() && isAlphaPremultiplied())
      value = value*255/getAlpha(pixel);
    return value;
  }

  private final int extractAndScaleSample(int pixel, int component)
  {
    int field = pixel & getMask(component);
    int to8BitShift =
      8 - shifts[component] - getComponentSize(component);
    return (to8BitShift>0) ?
      (field << to8BitShift) :
      (field >>> (-to8BitShift));
  }
    

  /* FIXME: The Sun docs show that this method is overridden, but I don't
     see any way to improve on the superclass implementation. */
  public final int getRGB(int pixel) 
  {
    return super.getRGB(pixel);
  }
  
  public int getRed(Object inData)
  {
    return getRed(getPixelFromArray(inData));
  }

  public int getGreen(Object inData)
  {
    return getGreen(getPixelFromArray(inData));
  }

  public int getBlue(Object inData)
  {
    return getBlue(getPixelFromArray(inData));
  }
    
  public int getAlpha(Object inData)
  {
    return getAlpha(getPixelFromArray(inData));
  }

  public int getRGB(Object inData)
  {
    return getRGB(getPixelFromArray(inData));
  }
    
  /**
   * Converts a normalized pixel int value in the sRGB color
   * space to an array containing a single pixel of the color space
   * of the color model.
   *
   * <p>This method performs the inverse function of
   * <code>getRGB(Object inData)</code>.
   *
   * @param rgb pixel as a normalized sRGB, 0xAARRGGBB value.
   *  
   * @param pixel to avoid needless creation of arrays, an array to
   * use to return the pixel can be given. If null, a suitable array
   * will be created.
   *
   * @return array of transferType containing a single pixel. The
   * pixel should be encoded in the natural way of the color model.
   *
   * @see #getRGB(Object)
   */
  public Object getDataElements(int rgb, Object pixel)
  {
    // FIXME: handle alpha multiply
    
    int pixelValue = 0;
    int a = 0;
    if (hasAlpha()) {
      a = (rgb >>> 24) & 0xff;
      pixelValue = valueToField(a, 3, 8);
    }
	
    if (hasAlpha() && isAlphaPremultiplied())
      {
	int r, g, b;
	/* if r=0xff and a=0xff, then resulting
	   value will be (r*a)>>>8 == 0xfe... This seems wrong.
	   We should divide by 255 rather than shifting >>>8 after
	   multiplying.
	   
	   Too bad, shifting is probably less expensive.
	   r = ((rgb >>> 16) & 0xff)*a;
	   g = ((rgb >>>  8) & 0xff)*a;
	   b = ((rgb >>> 0) & 0xff)*a; */
	/* The r, g, b values we calculate are 16 bit. This allows
	   us to avoid discarding the lower 8 bits obtained if
	   multiplying with the alpha band. */
	
	// using 16 bit values
	r = ((rgb >>> 8) & 0xff00)*a/255;
	g = ((rgb >>> 0) & 0xff00)*a/255;
	b = ((rgb <<  8) & 0xff00)*a/255;
	pixelValue |= 
	  valueToField(r, 0, 16) |  // Red
	  valueToField(g, 1, 16) |  // Green
	  valueToField(b, 2, 16);   // Blue
      }
    else
      {
	int r, g, b;
	// using 8 bit values
	r = (rgb >>> 16) & 0xff;
	g = (rgb >>>  8) & 0xff;
	b = (rgb >>>  0) & 0xff;
	
	pixelValue |= 
	  valueToField(r, 0, 8) |  // Red
	  valueToField(g, 1, 8) |  // Green
	  valueToField(b, 2, 8);   // Blue
      }
    
    /* In this color model, the whole pixel fits in the first element
       of the array. */
    DataBuffer buffer = Buffers.createBuffer(transferType, pixel, 1);
    buffer.setElem(0, pixelValue);
    return Buffers.getData(buffer);
  }
    
  /**
   * Converts a value to the correct field bits based on the
   * information derived from the field masks.
   *
   * @param highBit the position of the most significant bit in the
   * val parameter.
   */
  private final int valueToField(int val, int component, int highBit)
  {
    int toFieldShift = 
      getComponentSize(component) + shifts[component] - highBit;
    int ret = (toFieldShift>0) ?
      (val << toFieldShift) :
      (val >>> (-toFieldShift));
    return ret & getMask(component);
  }  

  /**
   * Converts a 16 bit value to the correct field bits based on the
   * information derived from the field masks.
   */
  private final int value16ToField(int val, int component)
  {
    int toFieldShift = getComponentSize(component) + shifts[component] - 16;
    return (toFieldShift>0) ?
      (val << toFieldShift) :
      (val >>> (-toFieldShift));
  }

  /**
   * Fills an array with the unnormalized component samples from a
   * pixel value. I.e. decompose the pixel, but not perform any
   * color conversion.
   */
  public final int[] getComponents(int pixel, int[] components, int offset)
  {
    int numComponents = getNumComponents();
    if (components == null) components = new int[offset + numComponents];
    
    for (int b=0; b<numComponents; b++)
      components[offset++] = (pixel&getMask(b)) >>> shifts[b];
	
    return components;
  }

  public final int[] getComponents(Object pixel, int[] components,
				   int offset)
  {
    return getComponents(getPixelFromArray(pixel), components, offset);
  }
  
  public final WritableRaster createCompatibleWritableRaster(int w, int h)
  {
    SampleModel sm = createCompatibleSampleModel(w, h);
    Point origin = new Point(0, 0);
    return Raster.createWritableRaster(sm, origin);	
  }

  public int getDataElement(int[] components, int offset)
  {
    int numComponents = getNumComponents();
    int pixelValue = 0;
    
    for (int c=0; c<numComponents; c++)
      pixelValue |= (components[offset++] << shifts[c]) & getMask(c);

    return pixelValue;
  }  

  public Object getDataElements(int[] components, int offset, Object obj)
  {
    /* In this color model, the whole pixel fits in the first element
       of the array. */
    int pixelValue = getDataElement(components, offset);

    DataBuffer buffer = Buffers.createBuffer(transferType, obj, 1);
    buffer.setElem(0, pixelValue);
    return Buffers.getData(buffer);
  }
    
  public ColorModel coerceData(WritableRaster raster,
			       boolean isAlphaPremultiplied)
  {
    if (this.isAlphaPremultiplied == isAlphaPremultiplied)
      return this;
	
    /* TODO: provide better implementation based on the
       assumptions we can make due to the specific type of the
       color model. */
    super.coerceData(raster, isAlphaPremultiplied);
	
    return new ComponentColorModel(cspace, bits, hasAlpha(),
				   isAlphaPremultiplied, // argument
				   transparency, transferType);
  } 

  public boolean isCompatibleRaster(Raster raster)
  {
    /* FIXME: the Sun docs say this method is overridden here, 
       but I don't see any way to improve upon the implementation
       in ColorModel. */
    return super.isCompatibleRaster(raster);
  }

  String stringParam()
  {
    return super.stringParam() +
      ", redMask=" + Integer.toHexString(getRedMask()) +
      ", greenMask=" + Integer.toHexString(getGreenMask()) +
      ", blueMask=" + Integer.toHexString(getBlueMask()) +
      ", alphaMask=" + Integer.toHexString(getAlphaMask());
  }

  public String toString()
  {
    /* FIXME: Again, docs say override, but how do we improve upon the
       superclass implementation? */
    return super.toString();
  }
}
