/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "QueryHolder.h"
#include "Errors.h"
#include "Log.h"
#include "MySQLConnection.h"
#include "PreparedStatement.h"
#include "QueryResult.h"

bool SQLQueryHolderBase::SetPreparedQueryImpl(std::size_t index, PreparedStatementBase* stmt)
{
    if (m_queries.size() <= index)
    {
        LOG_ERROR("sql.sql", "Query index ({}) out of range (size: {}) for prepared statement", uint32(index), (uint32)m_queries.size());
        return false;
    }

    m_queries[index].first = stmt;
    return true;
}

PreparedQueryResult SQLQueryHolderBase::GetPreparedResult(std::size_t index) const
{
    // Don't call to this function if the index is of a prepared statement
    ASSERT(index < m_queries.size(), "Query holder result index out of range, tried to access index {} but there are only {} results",
        index, m_queries.size());

    return m_queries[index].second;
}

void SQLQueryHolderBase::SetPreparedResult(std::size_t index, PreparedResultSet* result)
{
    if (result && !result->GetRowCount())
    {
        delete result;
        result = nullptr;
    }

    /// store the result in the holder
    if (index < m_queries.size())
        m_queries[index].second = PreparedQueryResult(result);
}

SQLQueryHolderBase::~SQLQueryHolderBase()
{
    for (std::pair<PreparedStatementBase*, PreparedQueryResult>& query : m_queries)
    {
        /// if the result was never used, free the resources
        /// results used already (getresult called) are expected to be deleted
        delete query.first;
    }
}

void SQLQueryHolderBase::SetSize(std::size_t size)
{
    /// to optimize push_back, reserve the number of queries about to be executed
    m_queries.resize(size);
}

SQLQueryHolderTask::~SQLQueryHolderTask() = default;

bool SQLQueryHolderTask::Execute()
{
    /// execute all queries in the holder and pass the results
    for (std::size_t i = 0; i < m_holder->m_queries.size(); ++i)
        if (PreparedStatementBase* stmt = m_holder->m_queries[i].first)
            m_holder->SetPreparedResult(i, m_conn->Query(stmt));

    m_result.set_value();
    return true;
}

bool SQLQueryHolderCallback::InvokeIfReady()
{
    if (m_future.valid() && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        m_callback(*m_holder);
        return true;
    }

    return false;
}
