#include "resultmodel.h"
#include "resultscheduler.h"
#include "../types.h"
#include "task/scheduledefinition.h"
#include "scheduler/scheduler.h"
#include "client.h"

#include <QPointer>
#include <QUuid>
#include <QHash>

class ResultModel::Private : public QObject
{
    Q_OBJECT

public:
    Private(ResultModel *q)
    : q(q)
    {
        scheduler = Client::instance()->scheduler();
    }

    ResultModel *q;

    QPointer<ResultScheduler> resultScheduler;
    QPointer<Scheduler> scheduler;

    QHash<TaskId, int> identToResult;
    ExtResultList results;

public slots:
    void resultAdded(const QVariantMap &result);
    void resultModified(const QVariantMap &result);
};

void ResultModel::Private::resultAdded(const QVariantMap &result)
{
    int position = results.size();

    q->beginInsertRows(QModelIndex(), position, position);
    identToResult.insert(TaskId(result.value("task_id").toInt()), position);
    results = resultScheduler->results();
    q->endInsertRows();
}

void ResultModel::Private::resultModified(const QVariantMap &result)
{
    int pos = identToResult.value(TaskId(result.value("task_id").toInt()));
    Q_ASSERT(pos != -1);

    results = resultScheduler->results();

    QModelIndex index = q->indexFromTaskId(TaskId(result.value("task_id").toInt()));
    Q_ASSERT(index.isValid());
    emit q->dataChanged(index, index);
}

ResultModel::ResultModel(QObject *parent)
: QAbstractTableModel(parent)
, d(new Private(this))
{
}

ResultModel::~ResultModel()
{
    delete d;
}

void ResultModel::setScheduler(ResultScheduler *scheduler)
{
    if (d->resultScheduler == scheduler)
    {
        return;
    }

    if (d->resultScheduler)
    {
        disconnect(d->resultScheduler.data(), SIGNAL(resultAdded(QVariantMap)), d, SLOT(resultAdded(QVariantMap)));
        disconnect(d->resultScheduler.data(), SIGNAL(resultModified(QVariantMap)), d, SLOT(resultModified(QVariantMap)));
    }

    d->resultScheduler = scheduler;

    if (d->resultScheduler)
    {
        connect(d->resultScheduler.data(), SIGNAL(resultAdded(QVariantMap)), d, SLOT(resultAdded(QVariantMap)));
        connect(d->resultScheduler.data(), SIGNAL(resultModified(QVariantMap)), d, SLOT(resultModified(QVariantMap)));
    }

    emit schedulerChanged();
    reset();
}

ResultScheduler *ResultModel::scheduler() const
{
    return d->resultScheduler;
}

QModelIndex ResultModel::indexFromTaskId(const TaskId &taskId) const
{
    int pos = d->identToResult.value(taskId);

    if (pos == -1)
    {
        return QModelIndex();
    }
    else
    {
        return createIndex(pos, 0);
    }
}

void ResultModel::reset()
{
    beginResetModel();

    if (!d->resultScheduler.isNull())
    {
        d->results = d->resultScheduler->results();
    }

    endResetModel();
}

QVariant ResultModel::get(int index) const
{
    if (index < 0 || index >= d->results.size())
    {
        return QVariant();
    }

    return d->results.at(index);
}

QHash<int, QByteArray> ResultModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames.insert(TaskIdRole, "taskId");
    roleNames.insert(DateTimeRole, "dateTime");
    roleNames.insert(ResultsRole, "results");
    roleNames.insert(MeasurementRole, "name");
    roleNames.insert(SecondColumnRole, "secondColumn");
    return roleNames;
}

int ResultModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return d->results.size();
}

int ResultModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 5;
}

QVariant ResultModel::data(const QModelIndex &index, int role) const
{
    const QVariantMap &result = d->results.at(index.row());

    if (role == Qt::DisplayRole)
    {
        role = TaskIdRole + index.column();
    }

    switch (role)
    {
    case TaskIdRole:
        return result.value("task_id").toInt();

    case DateTimeRole:
        return result.value("report_time");
        //case ResultsRole: return report->results(); // TODO: Scripting can't do anything with that

    case MeasurementRole:
        return d->scheduler->taskByTaskId(TaskId(result.value("task_id").toInt())).method().toUpper();

    case SecondColumnRole:
        QVariantMap map = d->scheduler->taskByTaskId(TaskId(result.value("task_id").toInt())).toVariant().toMap();
        QVariantMap options = map.value("options").toMap();
        QString out;
        QStringList names;

        // option names for the second column may vary
        names << "host" << "url";

        foreach (const QString &name, names)
        {
            if (options.contains(name))
            {
                out = options.value(name).toString();
                break;
            }
        }

        // cut off last bit if it's too long
        if (out.length() > 35)
        {
            out = out.mid(0, 32) + "...";
        }

        return out;
    }

    return QVariant();
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
    {
        return QVariant();
    }

    section += TaskIdRole;

    switch (section)
    {
    case TaskIdRole:
        return tr("Task Id");

    case DateTimeRole:
        return tr("Date-Time");

    case ResultsRole:
        return tr("Results");

    default:
        return QVariant();
    }
}

#include "resultmodel.moc"
