/* SimpleDateFormat.java -- A class for parsing/formating simple 
   date constructs
   Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.
 
GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

As a special exception, if you link this library with other files to
produce an executable, this library does not by itself cause the
resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why the
executable file might be covered by the GNU General Public License. */


package java.text;

import java.util.Calendar;
import java.util.Date;
import java.util.Enumeration;
import java.util.GregorianCalendar;
import java.util.Locale;
import java.util.TimeZone;
import java.util.SimpleTimeZone;
import java.util.Vector;
import java.io.ObjectInputStream;
import java.io.IOException;

/**
 * SimpleDateFormat provides convenient methods for parsing and formatting
 * dates using Gregorian calendars (see java.util.GregorianCalendar). 
 */
public class SimpleDateFormat extends DateFormat 
{
  /** A pair class used by SimpleDateFormat as a compiled representation
   *  of a format string.
   */
  private class FieldSizePair 
  {
    public int field;
    public int size;

    /** Constructs a pair with the given field and size values */
    public FieldSizePair(int f, int s) {
      field = f;
      size = s;
    }
  }

  private transient Vector tokens;
  private DateFormatSymbols formatData;  // formatData
  private Date defaultCenturyStart = 
    new Date(System.currentTimeMillis() - (80*365*24*60*60*1000));
  private String pattern;
  private int serialVersionOnStream = 1; // 0 indicates JDK1.1.3 or earlier
  private static final long serialVersionUID = 4774881970558875024L;

  // This string is specified in the JCL.  We set it here rather than
  // do a DateFormatSymbols(Locale.US).getLocalPatternChars() since
  // someone could theoretically change those values (though unlikely).
  private static final String standardChars = "GyMdkHmsSEDFwWahKz";

  private void readObject(ObjectInputStream stream)
    throws IOException, ClassNotFoundException
  {
    stream.defaultReadObject();
    if (serialVersionOnStream < 1)
      {
        defaultCenturyStart =
	  new Date(System.currentTimeMillis() - (80*365*24*60*60*1000));
	serialVersionOnStream = 1;
      }

    // Set up items normally taken care of by the constructor.
    tokens = new Vector();
    compileFormat(pattern);
  }

  private void compileFormat(String pattern) 
  {
    // Any alphabetical characters are treated as pattern characters
    // unless enclosed in single quotes.

    char thisChar;
    int pos;
    int field;
    FieldSizePair current = null;

    for (int i=0; i<pattern.length(); i++) {
      thisChar = pattern.charAt(i);
      field = formatData.getLocalPatternChars().indexOf(thisChar);
      if (field == -1) {
	current = null;
	if (Character.isLetter(thisChar)) {
	  // Not a valid letter
	  tokens.addElement(new FieldSizePair(-1,0));
	} else if (thisChar == '\'') {
	  // Quoted text section; skip to next single quote
	  pos = pattern.indexOf('\'',i+1);
	  if (pos == -1) {
	    // This ought to be an exception, but spec does not
	    // let us throw one.
	    tokens.addElement(new FieldSizePair(-1,0));
	  }
	  if ((pos+1 < pattern.length()) && (pattern.charAt(pos+1) == '\'')) {
	    tokens.addElement(pattern.substring(i+1,pos+1));
	  } else {
	    tokens.addElement(pattern.substring(i+1,pos));
	  }
	  i = pos;
	} else {
	  // A special character
	  tokens.addElement(new Character(thisChar));
	}
      } else {
	// A valid field
	if ((current != null) && (field == current.field)) {
	  current.size++;
	} else {
	  current = new FieldSizePair(field,1);
	  tokens.addElement(current);
	}
      }
    }
  }
    
  public String toString() 
  {
    StringBuffer output = new StringBuffer();
    Enumeration e = tokens.elements();
    while (e.hasMoreElements()) {
      output.append(e.nextElement().toString());
    }
    return output.toString();
  }
      
  /**
   * Constructs a SimpleDateFormat using the default pattern for
   * the default locale.
   */
  public SimpleDateFormat() 
  {
    /*
     * There does not appear to be a standard API for determining 
     * what the default pattern for a locale is, so use package-scope
     * variables in DateFormatSymbols to encapsulate this.
     */
    super();
    Locale locale = Locale.getDefault();
    calendar = new GregorianCalendar(locale);
    tokens = new Vector();
    formatData = new DateFormatSymbols(locale);
    pattern = formatData.dateFormats[DEFAULT]+' '+formatData.timeFormats[DEFAULT];
    compileFormat(pattern);
    numberFormat = NumberFormat.getInstance(locale);
  }
  
  /**
   * Creates a date formatter using the specified pattern, with the default
   * DateFormatSymbols for the default locale.
   */
  public SimpleDateFormat(String pattern) 
  {
    this(pattern, Locale.getDefault());
  }

  /**
   * Creates a date formatter using the specified pattern, with the default
   * DateFormatSymbols for the given locale.
   */
  public SimpleDateFormat(String pattern, Locale locale) 
  {
    super();
    calendar = new GregorianCalendar(locale);
    tokens = new Vector();
    formatData = new DateFormatSymbols(locale);
    compileFormat(pattern);
    this.pattern = pattern;
    numberFormat = NumberFormat.getInstance(locale);
  }

  /**
   * Creates a date formatter using the specified pattern. The
   * specified DateFormatSymbols will be used when formatting.
   */
  public SimpleDateFormat(String pattern, DateFormatSymbols formatData) {
    super();
    calendar = new GregorianCalendar();
    // FIXME: XXX: Is it really necessary to set the timezone?
    // The Calendar constructor is supposed to take care of this.
    calendar.setTimeZone(TimeZone.getDefault());
    tokens = new Vector();
    this.formatData = formatData;
    compileFormat(pattern);
    this.pattern = pattern;
    numberFormat = NumberFormat.getInstance();
  }

  // What is the difference between localized and unlocalized?  The
  // docs don't say.

  /**
   * This method returns a string with the formatting pattern being used
   * by this object.  This string is unlocalized.
   *
   * @return The format string.
   */
  public String toPattern()
  {
    return pattern;
  }

  /**
   * This method returns a string with the formatting pattern being used
   * by this object.  This string is localized.
   *
   * @return The format string.
   */
  public String toLocalizedPattern()
  {
    String localChars = formatData.getLocalPatternChars();
    return applyLocalizedPattern (pattern, standardChars, localChars);
  }

  /**
   * This method sets the formatting pattern that should be used by this
   * object.  This string is not localized.
   *
   * @param pattern The new format pattern.
   */
  public void applyPattern(String pattern)
  {
    tokens = new Vector();
    compileFormat(pattern);
    this.pattern = pattern;
  }

  /**
   * This method sets the formatting pattern that should be used by this
   * object.  This string is localized.
   *
   * @param pattern The new format pattern.
   */
  public void applyLocalizedPattern(String pattern)
  {
    String localChars = formatData.getLocalPatternChars();
    pattern = applyLocalizedPattern (pattern, localChars, standardChars);
    applyPattern(pattern);
  }

  private String applyLocalizedPattern(String pattern,
				       String oldChars, String newChars)
  {
    int len = pattern.length();
    StringBuffer buf = new StringBuffer(len);
    boolean quoted = false;
    for (int i = 0;  i < len;  i++)
      {
	char ch = pattern.charAt(i);
	if (ch == '\'')
	  quoted = ! quoted;
	if (! quoted)
	  {
	    int j = oldChars.indexOf(ch);
	    if (j >= 0)
	      ch = newChars.charAt(j);
	  }
	buf.append(ch);
      }
    return buf.toString();
  }

  /** 
   * Returns the start of the century used for two digit years.
   *
   * @return A <code>Date</code> representing the start of the century
   * for two digit years.
   */
  public Date get2DigitYearStart()
  {
    return defaultCenturyStart;
  }

  /**
   * Sets the start of the century used for two digit years.
   *
   * @param date A <code>Date</code> representing the start of the century for
   * two digit years.
   */
  public void set2DigitYearStart(Date date)
  {
    defaultCenturyStart = date;
  }

  /**
   * This method returns the format symbol information used for parsing
   * and formatting dates.
   *
   * @return The date format symbols.
   */
  public DateFormatSymbols getDateFormatSymbols()
  {
    return formatData;
  }

  /**
   * This method sets the format symbols information used for parsing
   * and formatting dates.
   *
   * @param formatData The date format symbols.
   */
   public void setDateFormatSymbols(DateFormatSymbols formatData)
   {
     this.formatData = formatData;
   }

  /**
   * This methods tests whether the specified object is equal to this
   * object.  This will be true if and only if the specified object:
   * <p>
   * <ul>
   * <li>Is not <code>null</code>.
   * <li>Is an instance of <code>SimpleDateFormat</code>.
   * <li>Is equal to this object at the superclass (i.e., <code>DateFormat</code>)
   *     level.
   * <li>Has the same formatting pattern.
   * <li>Is using the same formatting symbols.
   * <li>Is using the same century for two digit years.
   * </ul>
   *
   * @param obj The object to compare for equality against.
   *
   * @return <code>true</code> if the specified object is equal to this object,
   * <code>false</code> otherwise.
   */
  public boolean equals(Object o)
  {
    if (o == null)
      return false;

    if (!super.equals(o))
      return false;

    if (!(o instanceof SimpleDateFormat))
      return false;

    SimpleDateFormat sdf = (SimpleDateFormat)o;

    if (!toPattern().equals(sdf.toPattern()))
      return false;

    if (!get2DigitYearStart().equals(sdf.get2DigitYearStart()))
      return false;

    if (!getDateFormatSymbols().equals(sdf.getDateFormatSymbols()))
      return false;

    return true;
  }


  /**
   * Formats the date input according to the format string in use,
   * appending to the specified StringBuffer.  The input StringBuffer
   * is returned as output for convenience.
   */
  public StringBuffer format(Date date, StringBuffer buffer, FieldPosition pos) {
    String temp;
    Calendar theCalendar = (Calendar) calendar.clone();
    theCalendar.setTime(date);
    
    // go through vector, filling in fields where applicable, else toString
    Enumeration e = tokens.elements();
    while (e.hasMoreElements()) {
      Object o = e.nextElement();
      if (o instanceof FieldSizePair) {
	FieldSizePair p = (FieldSizePair) o;
	int beginIndex = buffer.length();
	switch (p.field) {
	case ERA_FIELD:
	  buffer.append(formatData.eras[theCalendar.get(Calendar.ERA)]);
	  break;
	case YEAR_FIELD:
	  temp = String.valueOf(theCalendar.get(Calendar.YEAR));
	  if (p.size < 4)
	    buffer.append(temp.substring(temp.length()-2));
	  else
	    buffer.append(temp);
	  break;
	case MONTH_FIELD:
	  if (p.size < 3)
	    withLeadingZeros(theCalendar.get(Calendar.MONTH)+1,p.size,buffer);
	  else if (p.size < 4)
	    buffer.append(formatData.shortMonths[theCalendar.get(Calendar.MONTH)]);
	  else
	    buffer.append(formatData.months[theCalendar.get(Calendar.MONTH)]);
	  break;
	case DATE_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.DATE),p.size,buffer);
	  break;
	case HOUR_OF_DAY1_FIELD: // 1-24
	  withLeadingZeros(((theCalendar.get(Calendar.HOUR_OF_DAY)+23)%24)+1,p.size,buffer);
	  break;
	case HOUR_OF_DAY0_FIELD: // 0-23
	  withLeadingZeros(theCalendar.get(Calendar.HOUR_OF_DAY),p.size,buffer);
	  break;
	case MINUTE_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.MINUTE),p.size,buffer);
	  break;
	case SECOND_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.SECOND),p.size,buffer);
	  break;
	case MILLISECOND_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.MILLISECOND),p.size,buffer);
	  break;
	case DAY_OF_WEEK_FIELD:
	  if (p.size < 4)
	    buffer.append(formatData.shortWeekdays[theCalendar.get(Calendar.DAY_OF_WEEK)]);
	  else
	    buffer.append(formatData.weekdays[theCalendar.get(Calendar.DAY_OF_WEEK)]);
	  break;
	case DAY_OF_YEAR_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.DAY_OF_YEAR),p.size,buffer);
	  break;
	case DAY_OF_WEEK_IN_MONTH_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.DAY_OF_WEEK_IN_MONTH),p.size,buffer);
	  break;
	case WEEK_OF_YEAR_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.WEEK_OF_YEAR),p.size,buffer);
	  break;
	case WEEK_OF_MONTH_FIELD:
	  withLeadingZeros(theCalendar.get(Calendar.WEEK_OF_MONTH),p.size,buffer);
	  break;
	case AM_PM_FIELD:
	  buffer.append(formatData.ampms[theCalendar.get(Calendar.AM_PM)]);
	  break;
	case HOUR1_FIELD: // 1-12
	  withLeadingZeros(((theCalendar.get(Calendar.HOUR)+11)%12)+1,p.size,buffer);
	  break;
	case HOUR0_FIELD: // 0-11
	  withLeadingZeros(theCalendar.get(Calendar.HOUR),p.size,buffer);
	  break;
	case TIMEZONE_FIELD:
	  TimeZone zone = theCalendar.getTimeZone();
	  boolean isDST = theCalendar.get(Calendar.DST_OFFSET) != 0;
	  // FIXME: XXX: This should be a localized time zone.
	  String zoneID = zone.getDisplayName(isDST, p.size > 3 ? TimeZone.LONG : TimeZone.SHORT);
	  buffer.append(zoneID);
	  break;
	default:
	  throw new IllegalArgumentException("Illegal pattern character");
	}
	if (pos != null && p.field == pos.getField())
	  {
	    pos.setBeginIndex(beginIndex);
	    pos.setEndIndex(buffer.length());
	  }
      } else {
	buffer.append(o.toString());
      }
    }
    return buffer;
  }

  private void withLeadingZeros(int value, int length, StringBuffer buffer) {
    String valStr = String.valueOf(value);
    for (length -= valStr.length(); length > 0; length--)
      buffer.append('0');
    buffer.append(valStr);
  }

  private final boolean expect (String source, ParsePosition pos, char ch)
  {
    int x = pos.getIndex();
    boolean r = x < source.length() && source.charAt(x) == ch;
    if (r)
      pos.setIndex(x + 1);
    else
      pos.setErrorIndex(x);
    return r;
  }

  /**
   * This method parses the specified string into a date.
   * 
   * @param dateStr The date string to parse.
   * @param pos The input and output parse position
   *
   * @return The parsed date, or <code>null</code> if the string cannot be
   * parsed.
   */
  public Date parse (String dateStr, ParsePosition pos)
  {
    int fmt_index = 0;
    int fmt_max = pattern.length();

    calendar.clear();
    int quote_start = -1;
    for (; fmt_index < fmt_max; ++fmt_index)
      {
	char ch = pattern.charAt(fmt_index);
	if (ch == '\'')
	  {
	    int index = pos.getIndex();
	    if (fmt_index < fmt_max - 1
		&& pattern.charAt(fmt_index + 1) == '\'')
	      {
		if (! expect (dateStr, pos, ch))
		  return null;
		++fmt_index;
	      }
	    else
	      quote_start = quote_start < 0 ? fmt_index : -1;
	    continue;
	  }

	if (quote_start != -1
	    || ((ch < 'a' || ch > 'z')
		&& (ch < 'A' || ch > 'Z')))
	  {
	    if (! expect (dateStr, pos, ch))
	      return null;
	    continue;
	  }

	// We've arrived at a potential pattern character in the
	// pattern.
	int first = fmt_index;
	while (++fmt_index < fmt_max && pattern.charAt(fmt_index) == ch)
	  ;
	int count = fmt_index - first;
	--fmt_index;

	// We can handle most fields automatically: most either are
	// numeric or are looked up in a string vector.  In some cases
	// we need an offset.  When numeric, `offset' is added to the
	// resulting value.  When doing a string lookup, offset is the
	// initial index into the string array.
	int calendar_field;
	boolean is_numeric = true;
	String[] match = null;
	int offset = 0;
	int zone_number = 0;
	switch (ch)
	  {
	  case 'd':
	    calendar_field = Calendar.DATE;
	    break;
	  case 'D':
	    calendar_field = Calendar.DAY_OF_YEAR;
	    break;
	  case 'F':
	    calendar_field = Calendar.DAY_OF_WEEK_IN_MONTH;
	    break;
	  case 'E':
	    is_numeric = false;
	    offset = 1;
	    calendar_field = Calendar.DAY_OF_WEEK;
	    match = (count <= 3
		     ? formatData.getShortWeekdays()
		     : formatData.getWeekdays());
	    break;
	  case 'w':
	    calendar_field = Calendar.WEEK_OF_YEAR;
	    break;
	  case 'W':
	    calendar_field = Calendar.WEEK_OF_MONTH;
	    break;
	  case 'M':
	    calendar_field = Calendar.MONTH;
	    if (count <= 2)
	      offset = -1;
	    else
	      {
		is_numeric = false;
		match = (count <= 3
			 ? formatData.getShortMonths()
			 : formatData.getMonths());
	      }
	    break;
	  case 'y':
	    calendar_field = Calendar.YEAR;
	    if (count <= 2)
	      offset = 1900;
	    break;
	  case 'K':
	    calendar_field = Calendar.HOUR;
	    break;
	  case 'h':
	    calendar_field = Calendar.HOUR;
	    break;
	  case 'H':
	    calendar_field = Calendar.HOUR_OF_DAY;
	    break;
	  case 'k':
	    calendar_field = Calendar.HOUR_OF_DAY;
	    break;
	  case 'm':
	    calendar_field = Calendar.MINUTE;
	    break;
	  case 's':
	    calendar_field = Calendar.SECOND;
	    break;
	  case 'S':
	    calendar_field = Calendar.MILLISECOND;
	    break;
	  case 'a':
	    is_numeric = false;
	    calendar_field = Calendar.AM_PM;
	    match = formatData.getAmPmStrings();
	    break;
	  case 'z':
	    // We need a special case for the timezone, because it
	    // uses a different data structure than the other cases.
	    is_numeric = false;
	    calendar_field = Calendar.DST_OFFSET;
	    String[][] zoneStrings = formatData.getZoneStrings();
	    int zoneCount = zoneStrings.length;
	    int index = pos.getIndex();
	    boolean found_zone = false;
	    for (int j = 0;  j < zoneCount;  j++)
	      {
		String[] strings = zoneStrings[j];
		int k;
		for (k = 1; k < strings.length; ++k)
		  {
		    if (dateStr.startsWith(strings[k], index))
		      break;
		  }
		if (k != strings.length)
		  {
		    if (k > 2)
		      ;		// FIXME: dst.
		    zone_number = 0; // FIXME: dst.
		    // FIXME: raw offset to SimpleTimeZone const.
		    calendar.setTimeZone(new SimpleTimeZone (1, strings[0]));
		    pos.setIndex(index + strings[k].length());
		    break;
		  }
	      }
	    if (! found_zone)
	      {
		pos.setErrorIndex(pos.getIndex());
		return null;
	      }
	    break;
	  default:
	    pos.setErrorIndex(pos.getIndex());
	    return null;
	  }

	// Compute the value we should assign to the field.
	int value;
	if (is_numeric)
	  {
	    numberFormat.setMinimumIntegerDigits(count);
	    Number n = numberFormat.parse(dateStr, pos);
	    if (pos == null || ! (n instanceof Long))
	      return null;
	    value = n.intValue() + offset;
	  }
	else if (match != null)
	  {
	    int index = pos.getIndex();
	    int i;
	    for (i = offset; i < match.length; ++i)
	      {
		if (dateStr.startsWith(match[i], index))
		  break;
	      }
	    if (i == match.length)
	      {
		pos.setErrorIndex(index);
		return null;
	      }
	    pos.setIndex(index + match[i].length());
	    value = i;
	  }
	else
	  value = zone_number;

	// Assign the value and move on.
	calendar.set(calendar_field, value);
      }

    try
      {
        return calendar.getTime();
      }
    catch (IllegalArgumentException x)
      {
        pos.setErrorIndex(pos.getIndex());
	return null;
      }
  }
}
