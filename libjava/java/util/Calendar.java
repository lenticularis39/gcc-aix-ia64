/* java.util.Calendar
   Copyright (C) 1998, 1999, 2000 Free Software Foundation, Inc.

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


package java.util;
import java.lang.reflect.InvocationTargetException;
import java.io.*;

/**
 * This class is an abstract base class for Calendars, which can be
 * used to convert between <code>Date</code> objects and a set of
 * integer fields which represent <code>YEAR</code>,
 * <code>MONTH</code>, <code>DAY</code>, etc.  The <code>Date</code>
 * object represents a time in milliseconds since the Epoch. <br>
 * 
 * This class is locale sensitive.  To get the Object matching the
 * current locale you can use <code>getInstance</code>.  You can even provide
 * a locale or a timezone.  <code>getInstance</code> returns currently
 * a <code>GregorianCalendar</code> for the current date. <br>
 *
 * If you want to convert a date from the Year, Month, Day, DayOfWeek,
 * etc.  Representation to a <code>Date</code>-Object, you can create
 * a new Calendar with <code>getInstance()</code>,
 * <code>clear()</code> all fields, <code>set(int,int)</code> the
 * fields you need and convert it with <code>getTime()</code>. <br>
 *
 * If you want to convert a <code>Date</code>-object to the Calendar
 * representation, create a new Calendar, assign the
 * <code>Date</code>-Object with <code>setTime()</code>, and read the
 * fields with <code>get(int)</code>. <br>
 *
 * When computing the date from time fields, it may happen, that there
 * are either two few fields set, or some fields are inconsistent.  This
 * cases will handled in a calender specific way.  Missing fields are
 * replaced by the fields of the epoch: 1970 January 1 00:00. <br>
 *
 * To understand, how the day of year is computed out of the fields
 * look at the following table.  It is traversed from top to bottom,
 * and for the first line all fields are set, that line is used to
 * compute the day. <br>
 *
 * <pre>
 * month + day_of_month
 * month + week_of_month + day_of_week
 * month + day_of_week_of_month + day_of_week
 * day_of_year
 * day_of_week + week_of_year
 * </pre>
 * 
 * The hour_of_day-field takes precedence over the ampm and
 * hour_of_ampm fields. <br>
 *
 * <STRONG>Note:</STRONG> This can differ for non-Gregorian calendar. <br>
 *
 * To convert a calendar to a human readable form and vice versa,  use
 * the <code>java.text.DateFormat</code> class. <br>
 * 
 * Other useful things you can do with an calendar, is
 * <code>roll</code>ing fields (that means increase/decrease a
 * specific field by one, propagating overflows), or
 * <code>add</code>ing/substracting a fixed amount to a field.
 *
 * @see Date
 * @see GregorianCalendar
 * @see TimeZone
 * @see java.text.DateFormat 
 */
public abstract class Calendar implements Serializable, Cloneable
{
  /**
   * Constant representing the era time field.
   */
  public static final int ERA = 0;
  /**
   * Constant representing the year time field.
   */
  public static final int YEAR = 1;
  /**
   * Constant representing the month time field.  This field
   * should contain one of the JANUARY,...,DECEMBER constants below.
   */
  public static final int MONTH = 2;
  /**
   * Constant representing the week of the year field.
   * @see #setFirstDayOfWeek(int)
   */
  public static final int WEEK_OF_YEAR = 3;
  /**
   * Constant representing the week of the month time field.
   * @see #setFirstDayOfWeek(int)
   */
  public static final int WEEK_OF_MONTH = 4;
  /**
   * Constant representing the day time field, synonym for DAY_OF_MONTH.
   */
  public static final int DATE = 5;
  /**
   * Constant representing the day time field.
   */
  public static final int DAY_OF_MONTH = 5;
  /**
   * Constant representing the day of year time field.  This is
   * 1 for the first day in month.
   */
  public static final int DAY_OF_YEAR = 6;
  /**
   * Constant representing the day of week time field.  This field
   * should contain one of the SUNDAY,...,SATURDAY constants below.
   */
  public static final int DAY_OF_WEEK = 7;
  /**
   * Constant representing the day-of-week-in-month field.  For
   * instance this field contains 2 for the second thursday in a
   * month.  If you give a negative number here, the day will count
   * from the end of the month.
   */
  public static final int DAY_OF_WEEK_IN_MONTH = 8;
  /**
   * Constant representing the part of the day for 12-hour clock.  This
   * should be one of AM or PM.
   */
  public static final int AM_PM = 9;
  /**
   * Constant representing the hour time field for 12-hour clock.
   */
  public static final int HOUR = 10;
  /**
   * Constant representing the hour of day time field for 24-hour clock.
   */
  public static final int HOUR_OF_DAY = 11;
  /**
   * Constant representing the minute of hour time field.
   */
  public static final int MINUTE = 12;
  /**
   * Constant representing the second time field.
   */
  public static final int SECOND = 13;
  /**
   * Constant representing the millisecond time field.
   */
  public static final int MILLISECOND = 14;
  /**
   * Constant representing the time zone offset time field for the
   * time given in the other fields.  It is measured in
   * milliseconds.  The default is the offset of the time zone.  
   */
  public static final int ZONE_OFFSET = 15;
  /**
   * Constant representing the daylight saving time offset in
   * milliseconds.  The default is the value given by the time zone.  
   */
  public static final int DST_OFFSET = 16;
  /**
   * Number of time fields.
   */
  public static final int FIELD_COUNT = 17;

  /**
   * Constant representing Sunday.
   */
  public static final int SUNDAY = 1;
  /**
   * Constant representing Monday.
   */
  public static final int MONDAY = 2;
  /**
   * Constant representing Tuesday.
   */
  public static final int TUESDAY = 3;
  /**
   * Constant representing Wednesday.
   */
  public static final int WEDNESDAY = 4;
  /**
   * Constant representing Thursday.
   */
  public static final int THURSDAY = 5;
  /**
   * Constant representing Friday.
   */
  public static final int FRIDAY = 6;
  /**
   * Constant representing Saturday.
   */
  public static final int SATURDAY = 7;

  /**
   * Constant representing January.
   */
  public static final int JANUARY = 0;
  /**
   * Constant representing February.
   */
  public static final int FEBRUARY = 1;
  /**
   * Constant representing March.
   */
  public static final int MARCH = 2;
  /**
   * Constant representing April.
   */
  public static final int APRIL = 3;
  /**
   * Constant representing May.
   */
  public static final int MAY = 4;
  /**
   * Constant representing June.
   */
  public static final int JUNE = 5;
  /**
   * Constant representing July.
   */
  public static final int JULY = 6;
  /**
   * Constant representing August.
   */
  public static final int AUGUST = 7;
  /**
   * Constant representing September.
   */
  public static final int SEPTEMBER = 8;
  /**
   * Constant representing October.
   */
  public static final int OCTOBER = 9;
  /**
   * Constant representing November.
   */
  public static final int NOVEMBER = 10;
  /**
   * Constant representing December.
   */
  public static final int DECEMBER = 11;
  /**
   * Constant representing Undecimber. This is an artificial name useful
   * for lunar calendars.
   */
  public static final int UNDECIMBER = 12;

  /**
   * Useful constant for 12-hour clock.
   */
  public static final int AM = 0;
  /**
   * Useful constant for 12-hour clock.
   */
  public static final int PM = 1;

  /**
   * The time fields.  The array is indexed by the constants YEAR to
   * DST_OFFSET.
   * @serial
   */
  protected int[] fields = new int[FIELD_COUNT];
  /**
   * The flags which tell if the fields above have a value.
   * @serial
   */
  protected boolean[] isSet = new boolean[FIELD_COUNT];
  /**
   * The time in milliseconds since the epoch.
   * @serial
   */
  protected long time;
  /**
   * Tells if the above field has a valid value.
   * @serial
   */
  protected boolean isTimeSet;
  /**
   * Tells if the fields have a valid value.  This superseeds the isSet
   * array.
   * @serial
   */
  protected boolean areFieldsSet;

  /**
   * The time zone of this calendar.  Used by sub classes to do UTC / local
   * time conversion.  Sub classes can access this field with getTimeZone().
   * @serial
   */
  private TimeZone zone;

  /**
   * Specifies if the date/time interpretation should be lenient.
   * If the flag is set, a date such as "February 30, 1996" will be
   * treated as the 29th day after the February 1.  If this flag
   * is false, such dates will cause an exception.
   * @serial
   */
  private boolean lenient;

  /**
   * Sets what the first day of week is.  This is used for
   * WEEK_OF_MONTH and WEEK_OF_YEAR fields. 
   * @serial
   */
  private int firstDayOfWeek;

  /**
   * Sets how many days are required in the first week of the year.
   * If the first day of the year should be the first week you should
   * set this value to 1.  If the first week must be a full week, set
   * it to 7.
   * @serial
   */
  private int minimalDaysInFirstWeek;

  /**
   * The version of the serialized data on the stream. 
   * <dl><dt>0 or not present</dt>
   * <dd> JDK 1.1.5 or later.</dd>
   * <dl><dt>1</dt>
   * <dd>JDK 1.1.6 or later.  This always writes a correct `time' value
   * on the stream, as well as the other fields, to be compatible with
   * earlier versions</dd>
   * @since JDK1.1.6
   * @serial
   */
  private int serialVersionOnStream = 1;

  /**
   * XXX - I have not checked the compatibility.  The documentation of
   * the serialized-form is quite hairy...
   */
  static final long serialVersionUID = -1807547505821590642L;

  /**
   * The name of the resource bundle.
   */
  private static final String bundleName = "gnu.java.locale.Calendar";

  /**
   * Constructs a new Calender with the default time zone and the default
   * locale.
   */
  protected Calendar()
  {
    this(TimeZone.getDefault(), Locale.getDefault());
  }

  /**
   * Constructs a new Calender with the given time zone and the given
   * locale.
   * @param zone a time zone.
   * @param locale a locale.
   */
  protected Calendar(TimeZone zone, Locale locale)
  {
    this.zone = zone;
    lenient = true;

    ResourceBundle rb = ResourceBundle.getBundle(bundleName, locale);

    firstDayOfWeek = ((Integer) rb.getObject("firstDayOfWeek")).intValue();
    minimalDaysInFirstWeek =
      ((Integer) rb.getObject("minimalDaysInFirstWeek")).intValue();
  }

  /**
   * Creates a calendar representing the actual time, using the default
   * time zone and locale.
   */
  public static synchronized Calendar getInstance()
  {
    return getInstance(TimeZone.getDefault(), Locale.getDefault());
  }

  /**
   * Creates a calendar representing the actual time, using the given
   * time zone and the default locale.
   * @param zone a time zone.
   */
  public static synchronized Calendar getInstance(TimeZone zone)
  {
    return getInstance(zone, Locale.getDefault());
  }

  /**
   * Creates a calendar representing the actual time, using the default
   * time zone and the given locale.
   * @param locale a locale.
   */
  public static synchronized Calendar getInstance(Locale locale)
  {
    return getInstance(TimeZone.getDefault(), locale);
  }

  /**
   * Creates a calendar representing the actual time, using the given
   * time zone and locale.
   * @param zone a time zone.
   * @param locale a locale.
   */
  public static synchronized Calendar getInstance(TimeZone zone, Locale locale)
  {
    String calendarClassName = null;
    ResourceBundle rb = ResourceBundle.getBundle(bundleName, locale);
    calendarClassName = rb.getString("calendarClass");
    if (calendarClassName != null)
      {
	try
	  {
	    Class calendarClass = Class.forName(calendarClassName);
	    if (Calendar.class.isAssignableFrom(calendarClass))
	      {
		return (Calendar) calendarClass.getConstructor(
		  new Class[] { TimeZone.class, Locale.class}
		).newInstance(new Object[] {zone, locale} );
	      }
	  }
	catch (ClassNotFoundException ex) {}
	catch (IllegalAccessException ex) {}
	catch (NoSuchMethodException ex) {}
	catch (InstantiationException ex) {}
	catch (InvocationTargetException ex) {}
	// XXX should we ignore these errors or throw an exception ?
      }
    return new GregorianCalendar(zone, locale);
  }

  /**
   * Gets the set of locales for which a Calendar is availiable.
   * @exception MissingResourceException if locale data couldn't be found.
   * @return the set of locales.
   */
  public static synchronized Locale[] getAvailableLocales()
  {
    ResourceBundle rb = ResourceBundle.getBundle(bundleName,
						 new Locale("", ""));
    return (Locale[]) rb.getObject("availableLocales");
  }

  /**
   * Converts the time field values (<code>fields</code>) to
   * milliseconds since the epoch UTC (<code>time</code>).  Override
   * this method if you write your own Calendar.  */
  protected abstract void computeTime();

  /**
   * Converts the milliseconds since the epoch UTC
   * (<code>time</code>) to time fields
   * (<code>fields</code>). Override this method if you write your
   * own Calendar.  
   */
  protected abstract void computeFields();

  /**
   * Converts the time represented by this object to a
   * <code>Date</code>-Object.
   * @return the Date.
   */
  public final Date getTime()
  {
    if (!isTimeSet)
      computeTime();
    return new Date(time);
  }

  /**
   * Sets this Calender's time to the given Date.  All time fields
   * are invalidated by this method.
   */
  public final void setTime(Date date)
  {
    setTimeInMillis(date.getTime());
  }

  /**
   * Returns the time represented by this Calendar.
   * @return the time in milliseconds since the epoch.
   */
  protected long getTimeInMillis()
  {
    if (!isTimeSet)
      computeTime();
    return time;
  }

  /**
   * Sets this Calender's time to the given Time.  All time fields
   * are invalidated by this method.
   * @param time the time in milliseconds since the epoch
   */
  protected void setTimeInMillis(long time)
  {
    this.time = time;
    isTimeSet = true;
    computeFields();
  }

  /**
   * Gets the value of the specified field.  They are recomputed
   * if they are invalid.
   * @param field the time field. One of the time field constants.
   * @return the value of the specified field
   */
  public final int get(int field)
  {
    complete();
    return fields[field];
  }

  /**
   * Gets the value of the specified field. This method doesn't 
   * recompute the fields, if they are invalid.
   * @param field the time field. One of the time field constants.
   * @return the value of the specified field, undefined if
   * <code>areFieldsSet</code> or <code>isSet[field]</code> is false.
   */
  protected final int internalGet(int field)
  {
    return fields[field];
  }

  /**
   * Sets the time field with the given value.  This does invalidate
   * the time in milliseconds.
   * @param field the time field. One of the time field constants
   * @param value the value to be set.
   */
  public final void set(int field, int value)
  {
    if (!areFieldsSet)
      computeFields();
    isTimeSet = false;
    fields[field] = value;
    isSet[field] = true;
  }

  /**
   * Sets the fields for year, month, and date
   * @param year the year.
   * @param month the month, one of the constants JANUARY..UNDICEMBER.
   * @param date the day of the month
   */
  public final void set(int year, int month, int date)
  {
    if (!areFieldsSet)
      computeFields();
    isTimeSet = false;
    fields[YEAR] = year;
    fields[MONTH] = month;
    fields[DATE] = date;
    isSet[YEAR] = isSet[MONTH] = isSet[DATE] = true;
  }

  /**
   * Sets the fields for year, month, date, hour, and minute
   * @param year the year.
   * @param month the month, one of the constants JANUARY..UNDICEMBER.
   * @param date the day of the month
   * @param hour the hour of day.
   * @param minute the minute.
   */
  public final void set(int year, int month, int date, int hour, int minute)
  {
    set(year, month, date);
    fields[HOUR_OF_DAY] = hour;
    fields[MINUTE] = minute;
    isSet[HOUR_OF_DAY] = isSet[MINUTE] = true;
  }

  /**
   * Sets the fields for year, month, date, hour, and minute
   * @param year the year.
   * @param month the month, one of the constants JANUARY..UNDICEMBER.
   * @param date the day of the month
   * @param hour the hour of day.
   * @param minute the minute.
   * @param second the second.
   */
  public final void set(int year, int month, int date,
			int hour, int minute, int second)
  {
    set(year, month, date, hour, minute);
    fields[SECOND] = second;
    isSet[SECOND] = true;
  }

  /**
   * Clears the values of all the time fields.
   */
  public final void clear()
  {
    isTimeSet = false;
    areFieldsSet = false;
    for (int i = 0; i < FIELD_COUNT; i++)
      isSet[i] = false;
  }

  /**
   * Clears the values of the specified time field.
   * @param field the time field. One of the time field constants.
   */
  public final void clear(int field)
  {
    isTimeSet = false;
    areFieldsSet = false;
    isSet[field] = false;
  }

  /**
   * Determines if the specified field has a valid value.
   * @return true if the specified field has a value.
   */
  public final boolean isSet(int field)
  {
    return isSet[field];
  }

  /**
   * Fills any unset fields in the time field list
   * @return true if the specified field has a value.  
   */
  protected void complete()
  {
    if (!isTimeSet)
      computeTime();
    if (!areFieldsSet)
      computeFields();
  }

  /**
   * Compares the given calender with this.  
   * @param o the object to that we should compare.
   * @return true, if the given object is a calendar, that represents
   * the same time (but doesn't neccessary have the same fields).
   */
  public boolean equals(Object o)
  {
    return (o instanceof Calendar)
      && getTimeInMillis() == ((Calendar) o).getTimeInMillis();
  }

  /**
   * Returns a hash code for this calendar.
   * @return a hash code, which fullfits the general contract of 
   * <code>hashCode()</code>
   */
  public int hashCode()
  {
    long time = getTimeInMillis();
    return (int) ((time & 0xffffffffL) ^ (time >> 32));
  }

  /**
   * Compares the given calender with this.  
   * @param o the object to that we should compare.
   * @return true, if the given object is a calendar, and this calendar
   * represents a smaller time than the calender o.
   * @exception ClassCastException if o is not an calendar.
   * @since JDK1.2 you don't need to override this method
   */
  public boolean before(Object o)
  {
    return getTimeInMillis() < ((Calendar) o).getTimeInMillis();
  }

  /**
   * Compares the given calender with this.  
   * @param o the object to that we should compare.
   * @return true, if the given object is a calendar, and this calendar
   * represents a bigger time than the calender o.
   * @exception ClassCastException if o is not an calendar.
   * @since JDK1.2 you don't need to override this method
   */
  public boolean after(Object o)
  {
    return getTimeInMillis() > ((Calendar) o).getTimeInMillis();
  }

  /**
   * Adds the specified amount of time to the given time field.  The
   * amount may be negative to subtract the time.  If the field overflows
   * it does what you expect: Jan, 25 + 10 Days is Feb, 4.
   * @param field the time field. One of the time field constants.
   * @param amount the amount of time.
   */
  public abstract void add(int field, int amount);

  /**
   * Rolls the specified time field up or down.  This means add one
   * to the specified field, but don't change the other fields.  If
   * the maximum for this field is reached, start over with the 
   * minimum value.  <br>
   *
   * <strong>Note:</strong> There may be situation, where the other
   * fields must be changed, e.g rolling the month on May, 31. 
   * The date June, 31 is automatically converted to July, 1.
   * @param field the time field. One of the time field constants.
   * @param up the direction, true for up, false for down.
   */
  public abstract void roll(int field, boolean up);

  /**
   * Rolls up or down the specified time field by the given amount.
   * A negative amount rolls down.  The default implementation is
   * call <code>roll(int, boolean)</code> for the specified amount.
   *
   * Subclasses should override this method to do more intuitiv things.
   *
   * @param field the time field. One of the time field constants.
   * @param amount the amount to roll by, positive for rolling up,
   * negative for rolling down.  
   * @since JDK1.2
   */
  public void roll(int field, int amount)
  {
    while (amount > 0)
      {
	roll(field, true);
	amount--;
      }
    while (amount < 0)
      {
	roll(field, false);
	amount++;
      }
  }


  /**
   * Sets the time zone to the specified value.
   * @param zone the new time zone
   */
  public void setTimeZone(TimeZone zone)
  {
    this.zone = zone;
  }

  /**
   * Gets the time zone of this calendar
   * @return the current time zone.
   */
  public TimeZone getTimeZone()
  {
    return zone;
  }

  /**
   * Specifies if the date/time interpretation should be lenient.
   * If the flag is set, a date such as "February 30, 1996" will be
   * treated as the 29th day after the February 1.  If this flag
   * is false, such dates will cause an exception.
   * @param lenient true, if the date should be interpreted linient,
   * false if it should be interpreted strict.
   */
  public void setLenient(boolean lenient)
  {
    this.lenient = lenient;
  }

  /**
   * Tells if the date/time interpretation is lenient.
   * @return true, if the date should be interpreted linient,
   * false if it should be interpreted strict.
   */
  public boolean isLenient()
  {
    return lenient;
  }

  /**
   * Sets what the first day of week is.  This is used for
   * WEEK_OF_MONTH and WEEK_OF_YEAR fields. 
   * @param value the first day of week.  One of SUNDAY to SATURDAY.
   */
  public void setFirstDayOfWeek(int value)
  {
    firstDayOfWeek = value;
  }

  /**
   * Gets what the first day of week is.  This is used for
   * WEEK_OF_MONTH and WEEK_OF_YEAR fields. 
   * @return the first day of week.  One of SUNDAY to SATURDAY.
   */
  public int getFirstDayOfWeek()
  {
    return firstDayOfWeek;
  }

  /**
   * Sets how many days are required in the first week of the year.
   * If the first day of the year should be the first week you should
   * set this value to 1.  If the first week must be a full week, set
   * it to 7.
   * @param value the minimal days required in the first week.
   */
  public void setMinimalDaysInFirstWeek(int value)
  {
    minimalDaysInFirstWeek = value;
  }

  /**
   * Gets how many days are required in the first week of the year.
   * @return the minimal days required in the first week.
   * @see #setMinimalDaysInFirstWeek
   */
  public int getMinimalDaysInFirstWeek()
  {
    return minimalDaysInFirstWeek;
  }

  /**
   * Gets the smallest value that is allowed for the specified field.
   * @param field the time field. One of the time field constants.
   * @return the smallest value.
   */
  public abstract int getMinimum(int field);

  /**
   * Gets the biggest value that is allowed for the specified field.
   * @param field the time field. One of the time field constants.
   * @return the biggest value.
   */
  public abstract int getMaximum(int field);


  /**
   * Gets the greatest minimum value that is allowed for the specified field.
   * @param field the time field. One of the time field constants.
   * @return the greatest minimum value.
   */
  public abstract int getGreatestMinimum(int field);

  /**
   * Gets the smallest maximum value that is allowed for the
   * specified field.  For example this is 28 for DAY_OF_MONTH.
   * @param field the time field. One of the time field constants.
   * @return the least maximum value.  
   */
  public abstract int getLeastMaximum(int field);

  /**
   * Gets the actual minimum value that is allowed for the specified field.
   * This value is dependant on the values of the other fields.
   * @param field the time field. One of the time field constants.
   * @return the actual minimum value.
   * @since jdk1.2
   */
  // FIXME: XXX: Not abstract in JDK 1.2.
  // public abstract int getActualMinimum(int field);

  /**
   * Gets the actual maximum value that is allowed for the specified field.
   * This value is dependant on the values of the other fields.
   * @param field the time field. One of the time field constants.
   * @return the actual maximum value.  
   * @since jdk1.2
   */
  // FIXME: XXX: Not abstract in JDK 1.2.
  // public abstract int getActualMaximum(int field);

  /**
   * Return a clone of this object.
   */
  public Object clone()
  {
    try
      {
	Calendar cal = (Calendar) super.clone();
	cal.fields = (int[]) fields.clone();
	cal.isSet = (boolean[])isSet.clone();
	return cal;
      }
    catch (CloneNotSupportedException ex)
      {
	return null;
      }
  }

  private final static String[] fieldNames = {
    ",ERA=", ",YEAR=", ",MONTH=",
    ",WEEK_OF_YEAR=", ",WEEK_OF_MONTH=",
    ",DAY_OF_MONTH=", ",DAY_OF_YEAR=", ",DAY_OF_WEEK=",
    ",DAY_OF_WEEK_IN_MONTH=",
    ",AM_PM=", ",HOUR=", ",HOUR_OF_DAY=",
    ",MINUTE=", ",SECOND=", ",MILLISECOND=",
    ",ZONE_OFFSET=", ",DST_OFFSET="
  };


  /**
   * Returns a string representation of this object.  It is mainly
   * for debugging purposes and its content is implementation
   * specific.
   */
  public String toString()
  {
    StringBuffer sb = new StringBuffer();
    sb.append(getClass().getName()).append('[');
    sb.append("time=");
    if (isTimeSet)
      sb.append(time);
    else
      sb.append("?");
    sb.append(",zone=" + zone);
    sb.append(",areFieldsSet=" + areFieldsSet);
    for (int i = 0; i < FIELD_COUNT; i++)
      {
	sb.append(fieldNames[i]);
	if (isSet[i])
	  sb.append(fields[i]);
	else
	  sb.append("?");
      }
    sb.append(",lenient=").append(lenient);
    sb.append(",firstDayOfWeek=").append(firstDayOfWeek);
    sb.append(",minimalDaysInFirstWeek=").append(minimalDaysInFirstWeek);
    sb.append("]");
    return sb.toString();
  }

  /**
   * Saves the state of the object to the stream.  Ideally we would
   * only write the time field, but we need to be compatible with
   * earlier versions. <br>
   *
   * This doesn't write the JDK1.1 field nextStamp to the stream, as
   * I don't know what it is good for, and because the documentation
   * says, that it could be omitted.  */
  private void writeObject(ObjectOutputStream stream) throws IOException
  {
    if (!isTimeSet)
      computeTime();
    stream.defaultWriteObject();
  }

  /**
   * Reads the object back from stream (deserialization).
   */
  private void readObject(ObjectInputStream stream)
    throws IOException, ClassNotFoundException
  {
    stream.defaultReadObject();
    if (!isTimeSet)
      computeTime();

    if (serialVersionOnStream > 1)
      {
	// This is my interpretation of the serial number:
	// Sun wants to remove all fields from the stream someday
	// and will then increase the serialVersion number again.
	// We prepare to be compatible.

	fields = new int[FIELD_COUNT];
	isSet = new boolean[FIELD_COUNT];
	areFieldsSet = false;
      }
  }
}
