/* Copyright (C) 2000  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

package java.awt.image;

import java.awt.*;
import java.awt.color.*;
import java.util.*;

import gnu.gcj.awt.ComponentDataBlitOp;

/**
 * A buffered image always starts at coordinates (0, 0).
 *
 * The buffered image is not subdivided into multiple tiles. Instead,
 * the image consists of one large tile (0,0) with the width and
 * height of the image. This tile is always considered to be checked
 * out.
 * 
 * @author Rolf W. Rasmussen <rolfwr@ii.uib.no>
 */
public class BufferedImage extends java.awt.Image
    //implements java.awt.image.WritableRenderedImage
{
  public static final int TYPE_CUSTOM         =  0,
                          TYPE_INT_RGB        =  1,
                          TYPE_INT_ARGB       =  2,
                          TYPE_INT_ARGB_PRE   =  3,
                          TYPE_INT_BGR        =  4,
                          TYPE_3BYTE_BGR      =  5,
                          TYPE_4BYTE_ABGR     =  6,
                          TYPE_4BYTE_ABGR_PRE =  7,
                          TYPE_USHORT_565_RGB =  8,
                          TYPE_USHORT_555_RGB =  9,
                          TYPE_BYTE_GRAY      = 10,
                          TYPE_USHORT_GRAY    = 11,
                          TYPE_BYTE_BINARY    = 12,
                          TYPE_BYTE_INDEXED   = 13;
  
  final static int[] bits3 = { 8, 8, 8 };
  final static int[] bits4 = { 8, 8, 8 };
  final static int[] bits1byte = { 8 };
  final static int[] bits1ushort = { 16 };
  
  final static int[] masks_int = { 0x00ff0000,
				   0x0000ff00,
				   0x000000ff,
				   DataBuffer.TYPE_INT };
  final static int[] masks_565 = { 0xf800,
				   0x07e0,
				   0x001f,
				   DataBuffer.TYPE_USHORT};
  final static int[] masks_555 = { 0x7c00,
				   0x03e0,
				   0x001f,
				   DataBuffer.TYPE_USHORT};
  
  public BufferedImage(int w, int h, int type)
  {
    ColorModel cm = null;
    
    boolean alpha = false;
    boolean premultiplied = false;
    switch (type)
      {
      case TYPE_4BYTE_ABGR_PRE:
      case TYPE_INT_ARGB_PRE:
	premultiplied = true;
	// fall through
      case TYPE_INT_ARGB:
      case TYPE_4BYTE_ABGR:
	alpha = true;
      }
	
    ColorSpace cs = ColorSpace.getInstance(ColorSpace.CS_sRGB);
    switch (type)
      {
      case TYPE_INT_RGB:
      case TYPE_INT_ARGB:
      case TYPE_INT_ARGB_PRE:
      case TYPE_USHORT_565_RGB:
      case TYPE_USHORT_555_RGB:
	int[] masks = null;
	switch (type)
	  {
	  case TYPE_INT_RGB:
	  case TYPE_INT_ARGB:
	  case TYPE_INT_ARGB_PRE:
	    masks = masks_int;
	    break;
	  case TYPE_USHORT_565_RGB:
	    masks = masks_565;
	    break;
	  case TYPE_USHORT_555_RGB:
	    masks = masks_555;
	    break;
	  }
	
	cm = new DirectColorModel(cs,
				  32, // 32 bits in an int
				  masks[0], // r
				  masks[1], // g
				  masks[2], // b
				  alpha ? 0xff000000 : 0,
				  premultiplied,
				  masks[3] // data type
				  );
	break;
	
      case TYPE_INT_BGR:
	String msg =
	  "FIXME: Programmer is confused. Why (and how) does a " +
	  "TYPE_INT_BGR image use ComponentColorModel to store " +
	  "8-bit values? Is data type TYPE_INT or TYPE_BYTE. What " +
	  "is the difference between TYPE_INT_BGR and TYPE_3BYTE_BGR?";
	throw new UnsupportedOperationException(msg);
	
      case TYPE_3BYTE_BGR:
      case TYPE_4BYTE_ABGR:
      case TYPE_4BYTE_ABGR_PRE:
      case TYPE_BYTE_GRAY:
      case TYPE_USHORT_GRAY:
	int[] bits = null;
	int dataType = DataBuffer.TYPE_BYTE;
	switch (type) {
	case TYPE_3BYTE_BGR:
	  bits = bits3;
	  break;
	case TYPE_4BYTE_ABGR:
	case TYPE_4BYTE_ABGR_PRE:
	  bits = bits4;
	  break;
	case TYPE_BYTE_GRAY:
	  bits = bits1byte;
	  break;
	case TYPE_USHORT_GRAY:
	  bits = bits1ushort;
	  dataType = DataBuffer.TYPE_USHORT;
	  break;
	}
	cm = new ComponentColorModel(cs, bits, alpha, premultiplied,
				     alpha ?
				     Transparency.TRANSLUCENT:
				     Transparency.OPAQUE,
				     dataType);
	break;
      case TYPE_BYTE_BINARY:
	byte[] vals = { 0, (byte) 0xff };
	cm = new IndexColorModel(8, 2, vals, vals, vals);
	break;
      case TYPE_BYTE_INDEXED:
	String msg2 = "type not implemented yet";
	throw new UnsupportedOperationException(msg2);
	// FIXME: build color-cube and create color model
      }
    
    init(cm,
	 cm.createCompatibleWritableRaster(w, h),
	 premultiplied,
	 null, // no properties
	 type
	 );
  }

  public BufferedImage(int w, int h, int type,
		       IndexColorModel indexcolormodel)
  {
    if ((type != TYPE_BYTE_BINARY) && (type != TYPE_BYTE_INDEXED))
      throw new IllegalArgumentException("type must be binary or indexed");

    init(indexcolormodel,
	 indexcolormodel.createCompatibleWritableRaster(w, h),
	 false, // not premultiplied (guess)
	 null, // no properties
	 type);
  }

  public BufferedImage(ColorModel colormodel, 
		       WritableRaster writableraster,
		       boolean premultiplied,
		       Hashtable properties)
  {
    init(colormodel, writableraster, premultiplied, properties,
	 TYPE_CUSTOM);
    // TODO: perhaps try to identify type?
  }
 
  WritableRaster raster;
  ColorModel colorModel;
  Hashtable properties;
  boolean isPremultiplied;
  int type;
  
  private void init(ColorModel cm,
		    WritableRaster writableraster,
		    boolean premultiplied,
		    Hashtable properties,
		    int type)
  {
    raster = writableraster;
    colorModel = cm;
    this.properties = properties;
    isPremultiplied = premultiplied;
    this.type = type;
  }
    
  //public void addTileObserver(TileObserver tileobserver) {}
  
  public void coerceData(boolean premultiplied)
  {
    colorModel = colorModel.coerceData(raster, premultiplied);
  }

  public WritableRaster copyData(WritableRaster dest)
  {
    if (dest == null)
      dest = raster.createCompatibleWritableRaster();

    int x = dest.getMinX();
    int y = dest.getMinY();
    int w = dest.getWidth();
    int h = dest.getHeight();
    
    // create a src child that has the right bounds...
    WritableRaster src =
      raster.createWritableChild(x, y, w, h, x, y,
				 null  // same bands
				 );
    
    // Refer to ComponentDataBlitOp for optimized data blitting:
    ComponentDataBlitOp.INSTANCE.filter(src, dest);
    return dest;
  }

  public Graphics2D createGraphics()
  {
    throw new UnsupportedOperationException("not implemented");
    // will require a lot of effort to implement
  }

  public void flush() {
  }
  
  public WritableRaster getAlphaRaster()
  {
    return colorModel.getAlphaRaster(raster);
  }
  
  public ColorModel getColorModel()
  {
    return colorModel;
  }
  
  public Raster getData()
  {
    return copyData(null);
    /* TODO: this might be optimized by returning the same
       raster (not writable) as long as image data doesn't change. */
  }

  public Raster getData(Rectangle rectangle)
  {
    WritableRaster dest =
      raster.createCompatibleWritableRaster(rectangle);
    return copyData(dest);
  }
  
  public Graphics getGraphics()
  {
    return createGraphics();
  }

  public int getHeight()
  {
    return raster.getHeight();
  }
  
  public int getHeight(ImageObserver imageobserver)
  {
    return getHeight();
  }
    
  public int getMinTileX()
  {
    return 0;
  }
  
  public int getMinTileY()
  {
    return 0;
  }

  public int getMinX()
  {
    return 0; 
  }

  public int getMinY() 
  {
    return 0;
  }
  
  public int getNumXTiles()
  {
    return 1;
  }

  public int getNumYTiles()
  {
	return 1;
  }

  public Object getProperty(String string)
  {
    if (properties == null)
      return null;
    return properties.get(string);
  }

  public Object getProperty(String string, ImageObserver imageobserver)
  {
    return getProperty(string);
  }

  
  public String[] getPropertyNames()
  {
    // FIXME: implement
    return null;
  }

  public int getRGB(int x, int y)
  {
    Object rgbElem = raster.getDataElements(x, y,
					    null // create as needed
					    );
    return colorModel.getRGB(rgbElem);
  }
    
  public int[] getRGB(int startX, int startY, int w, int h,
		      int[] rgbArray,
		      int offset, int scanlineStride)
  {
    if (rgbArray == null)
    {
      /*
	000000000000000000
	00000[#######-----   [ = start
	-----########-----   ] = end
	-----#######]00000
	000000000000000000  */
      int size = (h-1)*scanlineStride + w;
      rgbArray = new int[size];
    }
	
    int endX = startX + w;
    int endY = startY + h;
    
    /* *TODO*:
       Opportunity for optimization by examining color models...
       
       Perhaps wrap the rgbArray up in a WritableRaster with packed
       sRGB color model and perform optimized rendering into the
       array. */

    Object rgbElem = null;
    for (int y=startY; y<endY; y++)
      {
	int xoffset = offset;
	for (int x=startX; x<endX; x++)
	  {
	    int rgb;
	    rgbElem = raster.getDataElements(x, y, rgbElem);
	    rgb = colorModel.getRGB(rgbElem);
	    rgbArray[xoffset++] = rgb;
	  }
	offset += scanlineStride;
      }
    return rgbArray;
  }

  public WritableRaster getRaster()
  {
    return raster;
  }
  
  public SampleModel getSampleModel()
  {
    return raster.getSampleModel();
  }
    
  public ImageProducer getSource()
  {
    throw new UnsupportedOperationException("not implemented");
  }
  
  public Vector getSources()
  {
    return null;
  }
  
  public BufferedImage getSubimage(int x, int y, int w, int h)
  {
    WritableRaster subRaster = 
      getRaster().createWritableChild(x, y, w, h, 0, 0, null);
    
    return new BufferedImage(getColorModel(),
			     subRaster,
			     isPremultiplied,
			     properties);
  }

  public Raster getTile(int tileX, int tileY)
  {
    return getWritableTile(tileX, tileY);
  }
    
  public int getTileGridXOffset()
  {
    return 0; // according to javadocs
  }

  public int getTileGridYOffset()
  {
    return 0; // according to javadocs
  }

  public int getTileHeight()
  {
    return getHeight(); // image is one big tile
  }

  public int getTileWidth()
  {
    return getWidth(); // image is one big tile
  }

  public int getType()
  {
    return type;
  }

  public int getWidth()
  {
    return raster.getWidth();
  }

  public int getWidth(ImageObserver imageobserver)
  {
    return getWidth();
  }

  public WritableRaster getWritableTile(int tileX, int tileY)
  {
    isTileWritable(tileX, tileY);  // for exception
    return raster;
  }

  private static final Point[] tileIndices = { new Point() };
    
  public Point[] getWritableTileIndices()
  {
    return tileIndices;
  }

  public boolean hasTileWriters()
  {
    return true;
  }
  
  public boolean isAlphaPremultiplied()
  {
    return isPremultiplied;
  }

  public boolean isTileWritable(int tileX, int tileY)
  {
    if ((tileX != 0) || (tileY != 0))
      throw new ArrayIndexOutOfBoundsException("only tile is (0,0)");
    return true;
  }

  public void releaseWritableTile(int tileX, int tileY)
  {
    isTileWritable(tileX, tileY);  // for exception
  }

  //public void removeTileObserver(TileObserver tileobserver) {}

  public void setData(Raster src)
  {
    int x = src.getMinX();
    int y = src.getMinY();
    int w = src.getWidth();
    int h = src.getHeight();
    
    // create a dest child that has the right bounds...
    WritableRaster dest =
      raster.createWritableChild(x, y, w, h, x, y,
				 null  // same bands
				 );
    
    // Refer to ComponentDataBlitOp for optimized data blitting:
    ComponentDataBlitOp.INSTANCE.filter(src, dest);
  }

  public void setRGB(int x, int y, int argb)
  {
    Object rgbElem = colorModel.getDataElements(argb, null);
    raster.setDataElements(x, y, rgbElem);
  }
  
  public void setRGB(int startX, int startY, int w, int h,
		     int[] argbArray, int offset, int scanlineStride)
  {
    int endX = startX + w;
    int endY = startY + h;
    
    Object rgbElem = null;
    for (int y=startY; y<endY; y++)
      {
	int xoffset = offset;
	for (int x=startX; x<endX; x++)
	  {
	    int argb = argbArray[xoffset++];
	    rgbElem = colorModel.getDataElements(argb, rgbElem);
	    raster.setDataElements(x, y, rgbElem);
	  }
	offset += scanlineStride;    
      }
  }
    
  public String toString()
  {
    // FIXME: implement:
    return super.toString();
  }
}
