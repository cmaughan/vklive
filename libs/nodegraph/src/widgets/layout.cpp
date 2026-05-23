#include <zest/logger/logger.h>

#include <nodegraph/canvas.h>
#include <nodegraph/theme.h>
#include <nodegraph/widgets/layout.h>

namespace NodeGraph {

Layout::Layout(LayoutType type)
    : Widget("Layout")
{
    m_layoutType = type;
}

// Useful for indexing into glm::vec2
int Layout::GetAxisIndex(Axis axis) const
{
    switch (m_layoutType)
    {
    case LayoutType::Vertical:
        if (axis == Axis::Major)
        {
            return 1;
        }
        else
        {
            return 0;
        }
        break;
    case LayoutType::Horizontal:
        if (axis == Axis::Major)
        {
            return 0;
        }
        else
        {
            return 1;
        }
        break;
    }
    assert(!"Invalid");
    return 0;
}

float Layout::SpaceForWidgets(size_t count) const
{
    // Add space between all widgets
    float spacing = 0.0f;
    if (count > 0)
    {
        spacing += ((count - 1) * m_spacing);
    }
    return spacing;
}

tWidgets Layout::GetNonFixedWidgets() const
{
    tWidgets ret;
    std::copy_if(m_children.begin(), m_children.end(), std::back_inserter(ret), [](auto pWidget) {
        return !(pWidget->GetFlags() & WidgetFlags::DoNotLayout);
    });
    return ret;
}

void Layout::Update()
{
    tWidgets layoutWidgets = GetNonFixedWidgets();
    if (layoutWidgets.empty())
    {
        return;
    }

    // Find out the total max width/height the child widgets would like to be
    SizeHint hint;
    GetChildrenSizeHint(hint);

    float totalFixedSize = 0;
    float variableCount = 0;

    for (auto& pWidget : layoutWidgets)
    {
        glm::uvec2 constraints = pWidget->GetConstraints();
        NRectf widgetRect = pWidget->GetRectWithPad();

        switch (m_layoutType)
        {
        case LayoutType::Vertical:
            if (constraints.y == LayoutConstraint::Preferred)
            {
                totalFixedSize += widgetRect.Height();
            }
            else
            {
                variableCount++;
            }
            break;
        case LayoutType::Horizontal:
            if (constraints.x == LayoutConstraint::Preferred)
            {
                totalFixedSize += widgetRect.Width();
            }
            else
            {
                variableCount++;
            }
            break;
        }
    }

    auto spacingSize = SpaceForWidgets(layoutWidgets.size());

    // The rectangle for our layout contains all widgets plus our own padding
    // The layout rectangle grows in the minor axis but not the major one; it is fixed at whatever size it was when it was stacked
    auto contentMargins = GetContentsMargins();

    float availableSize = 0;
    NRectf layoutRect = GetRect();

    switch (m_layoutType)
    {
    case LayoutType::Vertical:
        // Set size of owner
        // The 'upper' layout will have sorted us into even rects, but here is our chance to constrain around the found widgets.
        if (m_constraints.y == LayoutConstraint::Preferred) // Expand to fill space vertically, or constrain?
        {
            layoutRect.SetSize(m_constraints.x == LayoutConstraint::Expanding ? hint.hint.x + contentMargins.x + contentMargins.z : layoutRect.Width(), layoutRect.Height());
        }
        availableSize = layoutRect.Height() - totalFixedSize - contentMargins.y - contentMargins.w;
        break;
    case LayoutType::Horizontal:
        if (m_constraints.x == LayoutConstraint::Preferred)
        {
            layoutRect.SetSize(layoutRect.Width(), m_constraints.y == LayoutConstraint::Expanding ? hint.hint.y + contentMargins.y + contentMargins.w : layoutRect.Height());
        }
        availableSize = layoutRect.Width() - totalFixedSize - contentMargins.x - contentMargins.z;
        break;
    }

    // Rect is the max minor axis size + content margins
    // Major axis is whatever it was before
    m_rect = layoutRect;

    // Layout rect is now the inner rect; in child rect coordinates
    layoutRect = NRectf(0.0f, 0.0f, layoutRect.Width(), layoutRect.Height());
    layoutRect.Adjust(contentMargins.x, contentMargins.y, -contentMargins.z, -contentMargins.w);
    layoutRect.Validate();

    // Local World space
    m_innerRect = layoutRect.Adjusted(m_rect.TopLeft());

    // Resize all the widgets to fit
    float expandingWidgetSize = (availableSize - spacingSize) / variableCount;
    for (int i = 0; i < layoutWidgets.size(); i++)
    {
        auto& pWidget = layoutWidgets[i];

        NRectf widgetRect = pWidget->GetRectWithPad();

        glm::uvec2 constraints = pWidget->GetConstraints();
        auto space = ((i == (layoutWidgets.size() - 1)) ? 0.0f : m_spacing);

        // Move and resize the rectangle to the correct location
        switch (m_layoutType)
        {
        case LayoutType::Vertical:
            widgetRect.Move(glm::vec2(layoutRect.Left(), layoutRect.Top()));
            widgetRect.SetSize(constraints.x == LayoutConstraint::Expanding ? layoutRect.Width() : widgetRect.Width(), constraints.y == LayoutConstraint::Expanding ? expandingWidgetSize : widgetRect.Height());
            layoutRect.SetTop(widgetRect.Bottom() + space);
            break;
        case LayoutType::Horizontal:
            widgetRect.Move(glm::vec2(layoutRect.Left(), layoutRect.Top()));
            widgetRect.SetSize(constraints.x == LayoutConstraint::Expanding ? expandingWidgetSize : widgetRect.Width(), constraints.y == LayoutConstraint::Expanding ? layoutRect.Height() : widgetRect.Height());
            layoutRect.SetLeft(widgetRect.Right() + space);
            break;
        }
        pWidget->SetRectWithPad(widgetRect);
    }
}

void Layout::SetRect(const NRectf& sz)
{
    Widget::SetRect(sz);

    Update();
}

void Layout::SetRectWithPad(const NRectf& rc)
{
    auto cm = GetContentsMargins();
    auto newRc = rc.Adjusted(glm::vec4(cm.x, cm.y, -cm.z, -cm.w));
    Widget::SetRect(newRc);

    Update();
}

NRectf Layout::GetRectWithPad() const
{
    auto cm = GetContentsMargins();
    return m_rect.Adjusted(glm::vec4(-cm.x, -cm.y, cm.z, cm.w));
}

void Layout::AddChild(std::shared_ptr<Widget> spWidget)
{
    m_children.push_back(spWidget);
    spWidget->SetParent(this);
    SortWidgets();
}

void Layout::MoveChildToBack(std::shared_ptr<Widget> pWidget)
{
    auto itr = std::find_if(m_children.begin(),
        m_children.end(),
        [&](const auto& pFound) -> bool {
            return pFound.get() == pWidget.get();
        });

    if (itr != m_children.end())
    {
        m_children.erase(itr);
        m_children.insert(m_children.end(), pWidget);
    }
    SortWidgets();
}

void Layout::MoveChildToFront(std::shared_ptr<Widget> pWidget)
{
    auto itr = std::find_if(m_children.begin(),
        m_children.end(),
        [&](const auto& pFound) -> bool {
            return pFound.get() == pWidget.get();
        });

    if (itr != m_children.end())
    {
        m_children.erase(itr);
        m_children.insert(m_children.begin(), pWidget);
    }
    SortWidgets();
}

const WidgetList& Layout::GetFrontToBack() const
{
    return m_frontToBack;
}

const WidgetList& Layout::GetBackToFront() const
{
    return m_children;
}

void Layout::SortWidgets()
{
    m_frontToBack = m_children;
    std::reverse(m_frontToBack.begin(), m_frontToBack.end());
}

const WidgetList& Layout::GetChildren() const
{
    return m_children;
}

void Layout::Draw(Canvas& canvas)
{
    auto& settings = Zest::GlobalSettingsManager::Instance();
    auto theme = settings.GetCurrentTheme();
    if (settings.GetBool(theme, b_debugShowLayout))
    {
        if (m_layoutType == LayoutType::Horizontal)
        {
            canvas.FillRect(ToWorldRect(GetRectWithPad()), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f));
            canvas.FillRect(ToWorldRect(m_rect), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f));
        }
        else
        {
            canvas.FillRect(ToWorldRect(GetRectWithPad()), glm::vec4(0.2f, 0.2f, 1.0f, 1.0f));
            canvas.FillRect(ToWorldRect(m_rect), glm::vec4(0.5f, 0.2f, 0.5f, 1.0f));
        }
    }
    for (auto& child : GetLayout()->GetBackToFront())
    {
        child->Draw(canvas);
    }
}

Layout* Layout::GetLayout()
{
    return this;
}

void Layout::SetContentsMargins(const glm::vec4& contentsMargins)
{
    m_contentsMargins = contentsMargins;
}

const glm::vec4& Layout::GetContentsMargins() const
{
    return m_contentsMargins;
}

LayoutType Layout::GetLayoutType() const
{
    return m_layoutType;
}

void Layout::SetSpacing(float val)
{
    m_spacing = val;
}

// For the direct children of this layout, get the size hint.
// This is the required size for the widgets
void Layout::GetChildrenSizeHint(SizeHint& hint) const
{
    for (auto& spChild : m_children)
    {
        if (auto pLayout = dynamic_cast<Layout*>(spChild.get()))
        {
            pLayout->GetChildrenSizeHint(hint);
        }
        else
        {
            auto pad = spChild->GetPadding();
            auto padSize = glm::vec2(pad.x + pad.z, pad.y + pad.w);
            hint.hint = glm::max(spChild->GetSizeHint() + padSize, hint.hint);
        }
    }
}

// Find the range of sizes for the child layout
glm::vec4 Layout::GetChildrenMinMaxSize() const
{
    auto minMax = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    for (auto& spChild : m_children)
    {
        glm::vec4 childMinMax;
        if (auto pLayout = dynamic_cast<Layout*>(spChild.get()))
        {
            childMinMax = pLayout->GetChildrenMinMaxSize();
        }
        else
        {
            childMinMax = spChild->GetMinMaxSize();
        }
        minMax += childMinMax;
    }
    return minMax;
}

}
