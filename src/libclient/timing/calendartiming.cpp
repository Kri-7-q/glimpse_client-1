#include "calendartiming.h"
#include "types.h"
#include <algorithm>
#include "client.h"
#include "controller/ntpcontroller.h"

const QList<int> CalendarTiming::AllMonths = QList<int>()<<1<<2<<3<<4<<5<<6<<7<<8<<9<<10<<11<<12;
const QList<int> CalendarTiming::AllDaysOfWeek = QList<int>()<<1<<2<<3<<4<<5<<6<<7;
const QList<int> CalendarTiming::AllDaysOfMonth = QList<int>()<< 1<< 2<< 3<< 4<< 5<< 6<< 7<< 8<< 9<<10
                                                              <<11<<12<<13<<14<<15<<16<<17<<18<<19<<20
                                                              <<21<<22<<23<<24<<25<<26<<27<<28<<29<<30<<31;
const QList<int> CalendarTiming::AllHours = QList<int>()<< 0<< 1<< 2<< 3<< 4<< 5<< 6<< 7<< 8<< 9<<10
                                                            <<11<<12<<13<<14<<15<<16<<17<<18<<19<<20
                                                            <<21<<22<<23;
const QList<int> CalendarTiming::AllMinutes = QList<int>()<< 0<< 1<< 2<< 3<< 4<< 5<< 6<< 7<< 8<< 9<<10
                                                              <<11<<12<<13<<14<<15<<16<<17<<18<<19<<20
                                                              <<21<<22<<23<<24<<25<<26<<27<<28<<29<<30
                                                              <<31<<32<<33<<34<<35<<36<<37<<38<<39<<40
                                                              <<41<<42<<43<<44<<45<<46<<47<<48<<49<<50
                                                              <<51<<52<<53<<54<<55<<56<<57<<58<<59;
const QList<int> CalendarTiming::AllSeconds = CalendarTiming::AllMinutes;

class CalendarTiming::Private
{
public:
    QDateTime start;
    QDateTime end;
    QList<int> months; // default: 1-12
    QList<int> daysOfWeek; // default: 1-7, 1 = Monday, 7 = Sunday
    QList<int> daysOfMonth; // default 1-31
    QList<int> hours; // default: 0-23
    QList<int> minutes; // default: 0-59
    QList<int> seconds; // default: 0-59

    QTime findTime(QTime &referenceTime);
};

QTime CalendarTiming::Private::findTime(QTime &referenceTime)
{
    QListIterator<int> hourIter(hours);
    QListIterator<int> minuteIter(minutes);
    QListIterator<int> secondIter(seconds);

    int hour;
    int minute;
    int second;

    while (hourIter.hasNext()) // hours
    {
        hour = hourIter.next();
        if (hour >= referenceTime.hour())
        {
            // hour found
            while (minuteIter.hasNext()) // minutes
            {
                minute = minuteIter.next();
                if (minute >= referenceTime.minute())
                {
                    // minute found
                    while (secondIter.hasNext()) // seconds
                    {
                        second = secondIter.next();
                        if (second >= referenceTime.second())
                        {
                            // finally, second found
                            return QTime(hour, minute, second);
                        }

                        if (!secondIter.hasNext())
                        {
                            secondIter.toFront();
                            referenceTime.setHMS(referenceTime.hour(), referenceTime.minute(), 0);
                            break;
                        }
                    }
                }

                if (!minuteIter.hasNext())
                {
                    minuteIter.toFront();
                    referenceTime.setHMS(referenceTime.hour(), 0, 0);
                    break;
                }
            }
        }
    }

    referenceTime.setHMS(0, 0, 0);
    return QTime();
}

CalendarTiming::CalendarTiming(const QDateTime &start, const QDateTime &end, const QList<int> &months,
                               const QList<int> &daysOfWeek, const QList<int> &daysOfMonth,
                               const QList<int> &hours, const QList<int> &minutes,
                               const QList<int> &seconds)
: d(new Private)
{
    d->start = start;
    d->end = end;
    d->months = months;
    d->daysOfWeek = daysOfWeek;
    d->daysOfMonth = daysOfMonth;
    d->hours = hours;
    d->minutes = minutes;
    d->seconds = seconds;

    // make sure the lists are sorted
    std::sort(d->months.begin(), d->months.end());
    std::sort(d->daysOfWeek.begin(), d->daysOfWeek.end());
    std::sort(d->daysOfMonth.begin(), d->daysOfMonth.end());
    std::sort(d->hours.begin(), d->hours.end());
    std::sort(d->minutes.begin(), d->minutes.end());
    std::sort(d->seconds.begin(), d->seconds.end());
}

CalendarTiming::~CalendarTiming()
{
    delete d;
}

bool CalendarTiming::reset()
{
    m_lastExecution = Client::instance()->ntpController()->currentDateTime();

    return nextRun().isValid();
}

QDateTime CalendarTiming::nextRun(const QDateTime &tzero) const
{
    QDateTime nextRun;
    QDateTime now;

    if (tzero.isValid())
    {
        now = tzero;
    }
    else
    {
        now = Client::instance()->ntpController()->currentDateTime();
    }

    QDate date = now.date();
    QTime time = now.time();
    bool found = false;

    QListIterator<int> monthIter(d->months);
    QListIterator<int> dayIter(d->daysOfMonth);
    QDate nextRunDate;
    QTime nextRunTime;

    // Check if the start time is reached
    if (d->start.isValid() && d->start > now)
    {
        date.setDate(d->start.date().year(), d->start.date().month(), d->start.date().day());
        time.setHMS(d->start.time().hour(), d->start.time().minute(), d->start.time().second());
        now.setDate(date);
        now.setTime(time);
    }

    int year = date.year();
    int month = date.month();
    int day = date.day();

    while (!found) // years
    {
        while (monthIter.hasNext() && !found) // months
        {
            month = monthIter.next();
            if (QDate(year, month, 1) >= QDate(date.year(), date.month(), 1))
            {
                // change to first element
                dayIter.toFront();

                while (dayIter.hasNext() && !found) // days
                {
                    day = dayIter.next();
                    if (QDate(year, month, day) >= date)
                    {
                        // potential day found, check if day of week is okay
                        if (d->daysOfWeek.contains(QDate(year, month, day).dayOfWeek()))
                        {
                            // check if we have a matching time for that day
                            if ((nextRunDate = QDate(year, month, day)) != now.date())
                            {
                                // if the next run date is not today we can take the first items
                                // as this is the earliest allowed time on that day
                                nextRunTime = QTime(*d->hours.constBegin(), *d->minutes.constBegin(), *d->seconds.constBegin());
                                nextRun = QDateTime(nextRunDate, nextRunTime);
                                found = true;
                            }
                            else if ((nextRunTime = d->findTime(time)).isValid())
                            {
                                nextRun = QDateTime(nextRunDate, nextRunTime);

                                // check if the calculated next run was already executed
                                if (!m_lastExecution.isValid() || m_lastExecution.secsTo(nextRun) > 1)
                                {
                                    found = true;
                                }
                            }
                        }
                    }

                    // check if end of set reached (= end of month)
                    if (!dayIter.hasNext())
                    {
                        date.setDate(date.year(), date.month(), 1);
                        break;
                    }
                }
            }

            // end of set reached, the first element is the right
            if (!monthIter.hasNext())
            {
                monthIter.toFront();
                date.setDate(++year, 1, 1);
                break;
            }
        }

        // for saftey reasons
        if (year > now.date().year() + 2)
        {
            // no next run found
            return QDateTime();
        }
    }

    // Stop if we exceed the end time
    if (d->end.isValid() && d->end < nextRun)
    {
        return QDateTime();
    }

    return nextRun;
}

bool CalendarTiming::isValid() const
{
    if (!d->start.isValid() || !d->end.isValid() || d->months.isEmpty() || d->daysOfWeek.isEmpty()
                                                 || d->daysOfMonth.isEmpty() || d->hours.isEmpty()
                                                 || d->minutes.isEmpty() || d->seconds.isEmpty())
    {
        return false;
    }
    else
    {
        return true;
    }
}

QVariant CalendarTiming::toVariant() const
{
    QVariantMap hash;
    hash.insert("start", d->start);
    hash.insert("end", d->end);
    hash.insert("months", listToVariant(d->months));
    hash.insert("days_of_week", listToVariant(d->daysOfWeek));
    hash.insert("days_of_month", listToVariant(d->daysOfMonth));
    hash.insert("hours", listToVariant(d->hours));
    hash.insert("minutes", listToVariant(d->minutes));
    hash.insert("seconds", listToVariant(d->seconds));

    QVariantMap resultMap;
    resultMap.insert(type(), hash);
    return resultMap;
}

TimingPtr CalendarTiming::fromVariant(const QVariant &variant)
{
    QVariantMap hash = variant.toMap();

    return TimingPtr(new CalendarTiming(hash.value("start").toDateTime(),
                                        hash.value("end").toDateTime(),
                                        listFromVariant<int>(hash.value("months")),
                                        listFromVariant<int>(hash.value("days_of_week")),
                                        listFromVariant<int>(hash.value("days_of_month")),
                                        listFromVariant<int>(hash.value("hours")),
                                        listFromVariant<int>(hash.value("minutes")),
                                        listFromVariant<int>(hash.value("seconds"))));
}

QDateTime CalendarTiming::start() const
{
    return d->start;
}

QDateTime CalendarTiming::end() const
{
    return d->end;
}

QList<int> CalendarTiming::months() const
{
    return d->months;
}

QList<int> CalendarTiming::daysOfWeek() const
{
    return d->daysOfWeek;
}

QList<int> CalendarTiming::daysOfMonth() const
{
    return d->daysOfMonth;
}

QList<int> CalendarTiming::hours() const
{
    return d->hours;
}

QList<int> CalendarTiming::minutes() const
{
    return d->minutes;
}

QList<int> CalendarTiming::seconds() const
{
    return d->seconds;
}
QString CalendarTiming::type() const
{
    return "calendar";
}
