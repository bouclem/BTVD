// BitVoid Explorer - connects to node API when available, shows "In Build" otherwise.

const API_BASE = window.location.origin + ':8545/api'; // default explorer API port
const STATUS_URL = API_BASE + '/status';

// Try to connect to the API on load.
async function checkApiStatus() {
    try {
        const response = await fetch(STATUS_URL, { method: 'GET', timeout: 3000 });
        if (response.ok) {
            showExplorerContent();
            loadChainStatus();
            loadRecentBlocks();
            return;
        }
    } catch (e) {
        // API not available — show "In Build".
    }
    showInBuild();
}

function showInBuild() {
    document.getElementById('explorer-status').style.display = 'block';
    document.getElementById('explorer-content').style.display = 'none';
}

function showExplorerContent() {
    document.getElementById('explorer-status').style.display = 'none';
    document.getElementById('explorer-content').style.display = 'block';
}

async function loadChainStatus() {
    try {
        const response = await fetch(API_BASE + '/status');
        const data = await response.json();

        const statusHtml = `
            <div class="status-card">
                <h3>Chain Status</h3>
                <table>
                    <tr><td>Height</td><td>${data.height}</td></tr>
                    <tr><td>Difficulty</td><td>${data.difficulty}</td></tr>
                    <tr><td>Latest block</td><td>${data.latest_hash ? data.latest_hash.substring(0, 32) + '...' : 'N/A'}</td></tr>
                    <tr><td>Block time</td><td>${data.latest_timestamp}</td></tr>
                    <tr><td>Energy (last block)</td><td>${data.latest_energy} watt-seconds</td></tr>
                </table>
            </div>
        `;
        const statusDiv = document.getElementById('explorer-status');
        statusDiv.style.display = 'block';
        statusDiv.innerHTML = statusHtml;
    } catch (e) {
        console.error('Failed to load chain status:', e);
    }
}

async function loadRecentBlocks() {
    try {
        const response = await fetch(API_BASE + '/blocks?limit=20');
        const blocks = await response.json();

        const blocksList = document.getElementById('blocks-list');
        if (!blocks || blocks.length === 0) {
            blocksList.innerHTML = '<p class="status-detail">No blocks yet.</p>';
            return;
        }

        let html = '<table><thead><tr><th>Height</th><th>Hash</th><th>Energy</th><th>Miner</th></tr></thead><tbody>';
        for (const block of blocks) {
            html += `
                <tr>
                    <td>${block.index}</td>
                    <td><a href="#" onclick="searchBlock(${block.index}); return false;">${block.hash.substring(0, 24)}...</a></td>
                    <td>${block.energy_kwh} kWh</td>
                    <td>${block.miner_address.substring(0, 20)}...</td>
                </tr>
            `;
        }
        html += '</tbody></table>';
        blocksList.innerHTML = html;
    } catch (e) {
        console.error('Failed to load blocks:', e);
    }
}

async function searchBlock(height) {
    try {
        const response = await fetch(API_BASE + '/block/' + height);
        const block = await response.json();
        displayBlockDetail(block);
    } catch (e) {
        console.error('Failed to load block:', e);
    }
}

async function searchTransaction(hash) {
    try {
        const response = await fetch(API_BASE + '/tx/' + hash);
        const tx = await response.json();
        displayTransactionDetail(tx);
    } catch (e) {
        console.error('Failed to load transaction:', e);
    }
}

async function searchAddress(address) {
    try {
        const response = await fetch(API_BASE + '/address/' + address);
        const data = await response.json();
        displayAddressDetail(data);
    } catch (e) {
        console.error('Failed to load address:', e);
    }
}

function displayBlockDetail(block) {
    const results = document.getElementById('search-results');
    results.innerHTML = `
        <div class="status-card">
            <h3>Block #${block.index}</h3>
            <table>
                <tr><td>Hash</td><td>${block.hash}</td></tr>
                <tr><td>Previous</td><td>${block.previous_hash}</td></tr>
                <tr><td>Timestamp</td><td>${block.timestamp}</td></tr>
                <tr><td>Difficulty</td><td>${block.difficulty}</td></tr>
                <tr><td>Nonce</td><td>${block.nonce}</td></tr>
                <tr><td>Energy</td><td>${block.energy_kwh} kWh (${block.watt_seconds} Ws)</td></tr>
                <tr><td>Energy source</td><td>${block.energy_source}</td></tr>
                <tr><td>Miner</td><td>${block.miner_address}</td></tr>
                <tr><td>Transactions</td><td>${block.transactions ? block.transactions.length : 0}</td></tr>
            </table>
        </div>
    `;
}

function displayTransactionDetail(tx) {
    const results = document.getElementById('search-results');
    results.innerHTML = `
        <div class="status-card">
            <h3>Transaction</h3>
            <table>
                <tr><td>Hash</td><td>${tx.hash}</td></tr>
                <tr><td>Fee</td><td>${tx.fee} base units</td></tr>
                <tr><td>Inputs</td><td>${tx.inputs ? tx.inputs.length : 0}</td></tr>
                <tr><td>Outputs</td><td>${tx.outputs ? tx.outputs.length : 0}</td></tr>
            </table>
        </div>
    `;
}

function displayAddressDetail(data) {
    const results = document.getElementById('search-results');
    results.innerHTML = `
        <div class="status-card">
            <h3>Address ${data.address}</h3>
            <table>
                <tr><td>Balance</td><td>${(data.balance / 100000000).toFixed(8)} BTVD</td></tr>
                <tr><td>Base units</td><td>${data.balance}</td></tr>
                <tr><td>UTXOs</td><td>${data.utxos ? data.utxos.length : 0}</td></tr>
            </table>
        </div>
    `;
}

// Search handler.
document.addEventListener('DOMContentLoaded', function() {
    const searchBtn = document.getElementById('search-btn');
    const searchInput = document.getElementById('search-input');

    if (searchBtn) {
        searchBtn.addEventListener('click', function() {
            const query = searchInput.value.trim();
            if (!query) return;

            // Detect query type: number = block, 40 chars = address, 64 chars = tx hash.
            if (/^\d+$/.test(query)) {
                searchBlock(parseInt(query));
            } else if (query.length === 40) {
                searchAddress(query);
            } else if (query.length === 64) {
                searchTransaction(query);
            } else {
                document.getElementById('search-results').innerHTML =
                    '<p class="status-detail">Invalid search. Enter a block height, address, or transaction hash.</p>';
            }
        });

        searchInput.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') searchBtn.click();
        });
    }

    // Check if API is available.
    checkApiStatus();
});
