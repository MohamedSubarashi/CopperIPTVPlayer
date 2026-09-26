#include "ui/widgets/GroupListWidget.h"

#include <QListView>
#include <QPainter>
#include <QPaintEvent>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

class GroupDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        // Leading glyph for the virtual entries.
        const int type = index.data(GroupListModel::TypeRole).toInt();
        QString glyph;
        if (type == GroupListModel::All)
            glyph = QStringLiteral("\u2630");
        else if (type == GroupListModel::Favorites)
            glyph = QStringLiteral("\u2605");

        QStyledItemDelegate::paint(painter, option, index);

        if (glyph.isEmpty())
            return;

        const bool selected = option.state & QStyle::State_Selected;
        const QFont font = option.font;
        const QRect glyphRect(option.rect.left() + 8,
                              option.rect.top() + (option.rect.height() - 16) / 2,
                              18, 16);
        painter->save();
        QFont g = font;
        g.setPixelSize(13);
        painter->setFont(g);
        const bool isFav = (type == GroupListModel::Favorites);
        painter->setPen(selected ? option.palette.color(QPalette::HighlightedText)
                                 : (isFav ? QColor(0xe8, 0xc1, 0x4f)
                                          : option.palette.color(QPalette::Text)));
        painter->drawText(glyphRect, Qt::AlignCenter, glyph);
        painter->restore();
    }
};

} // namespace

GroupListWidget::GroupListWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_view = new QListView(this);
    m_view->setUniformItemSizes(true);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setItemDelegate(new GroupDelegate(m_view));

    layout->addWidget(m_view);

    connect(m_view, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (m_model && index.isValid())
            emit groupActivated(m_model->itemAt(index.row()));
    });
}

void GroupListWidget::setModel(GroupListModel *model)
{
    m_model = model;
    m_view->setModel(model);
}

void GroupListWidget::selectRow(int row)
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return;
    m_view->setCurrentIndex(m_model->index(row, 0));
}

void GroupListWidget::selectAllChannels()
{
    if (m_model)
        selectRow(m_model->allRow());
}

void GroupListWidget::selectFavorites()
{
    if (m_model)
        selectRow(m_model->favoritesRow());
}

int GroupListWidget::currentRow() const
{
    return m_view->currentIndex().row();
}