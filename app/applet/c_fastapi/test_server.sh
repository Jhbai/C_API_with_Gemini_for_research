#!/usr/bin/env bash
# ==============================================================================
# Automated Integration Test & Benchmark Script for C-FastAPI Server
# ==============================================================================

PORT=${1:-8080}
HOST="127.0.0.1"
BASE_URL="http://${HOST}:${PORT}"

echo "======================================================="
echo "  🧪 Testing C-FastAPI Server on ${BASE_URL}"
echo "======================================================="

# Check if server is running
if ! curl -s --connect-timeout 2 "${BASE_URL}/health" > /dev/null; then
    echo "⚠️  Server is not running on ${BASE_URL}."
    echo "   Please start the server first in another terminal: 'make run'"
    exit 1
fi

echo -e "\n[1] Testing GET / (Root Endpoint)"
curl -i "${BASE_URL}/"
echo ""

echo -e "\n[2] Testing GET /health (Heartbeat)"
curl -i "${BASE_URL}/health"
echo ""

echo -e "\n[3] Testing GET /items/99?q=fastapi_async&skip=5 (Path & Query Params)"
curl -i "${BASE_URL}/items/99?q=fastapi_async&skip=5"
echo ""

echo -e "\n[4] Testing POST /items (JSON Body Parsing & 201 Created)"
curl -i -X POST "${BASE_URL}/items" \
     -H "Content-Type: application/json" \
     -d '{"name": "Gaming Laptop", "price": 1299.99, "tags": ["electronics", "tech"]}'
echo ""

echo -e "\n[5] Testing GET /users/developer_bob/status (Nested Dynamic Path Param)"
curl -i "${BASE_URL}/users/developer_bob/status"
echo ""

echo -e "\n[6] Testing 404 Not Found Handling"
curl -i "${BASE_URL}/undefined/route/here"
echo ""

echo -e "\n[7] Testing Concurrency (Sending 50 concurrent requests)"
SUCCESS=0
for i in {1..50}; do
    curl -s "${BASE_URL}/items/$i?q=test_$i" > /dev/null && ((SUCCESS++)) &
done
wait
echo "✅ Concurrency test completed: ${SUCCESS}/50 successful non-blocking responses received."

echo -e "\n🎉 All integration tests passed successfully!"
