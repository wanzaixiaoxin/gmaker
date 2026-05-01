let currentPage = 'list';
let editingName = null;
let editingVersions = [];
let currentConfig = null;

// ===== 导航 =====
document.querySelectorAll('.nav-item').forEach(el => {
  el.addEventListener('click', e => {
    e.preventDefault();
    const page = el.dataset.page;
    switchPage(page);
  });
});

function switchPage(page) {
  currentPage = page;
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.getElementById('page-' + page).classList.add('active');
  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
  document.querySelector(`.nav-item[data-page="${page}"]`).classList.add('active');

  const titles = { list: '配置列表', logs: '操作日志', edit: '编辑配置' };
  document.getElementById('page-title').textContent = titles[page] || '';

  if (page === 'list') loadConfigList();
  if (page === 'logs') loadLogs();
}

// ===== 配置列表 =====
function loadConfigList() {
  const tbody = document.getElementById('config-list-body');
  tbody.innerHTML = '<tr><td colspan="8" class="empty">加载中...</td></tr>';
  ConfigAPI.list().then(list => {
    if (!list || list.length === 0) {
      tbody.innerHTML = '<tr><td colspan="8" class="empty">暂无配置</td></tr>';
      return;
    }
    tbody.innerHTML = list.map(cfg => `
      <tr>
        <td><strong>${escapeHtml(cfg.name)}</strong></td>
        <td>${escapeHtml(cfg.namespace)}</td>
        <td>${escapeHtml(cfg.format)}</td>
        <td>${cfg.current_version}</td>
        <td>${statusTag(cfg.status)}</td>
        <td>${escapeHtml(cfg.description || '-')}</td>
        <td>${fmtTime(cfg.updated_at)}</td>
        <td>
          <button class="btn btn-sm" onclick="editConfig('${cfg.name}', '${cfg.namespace}')">编辑</button>
          <button class="btn btn-sm btn-danger" onclick="deleteConfig('${cfg.name}', '${cfg.namespace}')">删除</button>
        </td>
      </tr>
    `).join('');
  }).catch(err => {
    tbody.innerHTML = `<tr><td colspan="8" class="empty">加载失败: ${escapeHtml(err.message)}</td></tr>`;
  });
}

document.getElementById('btn-new-config').addEventListener('click', () => {
  editingName = null;
  editingVersions = [];
  currentConfig = null;
  document.getElementById('edit-name').value = '';
  document.getElementById('edit-namespace').value = 'default';
  document.getElementById('edit-format').value = 'json';
  document.getElementById('edit-desc').value = '';
  document.getElementById('edit-content').value = '';
  document.getElementById('version-list').innerHTML = '<li class="empty">暂无版本</li>';
  switchPage('edit');
});

document.getElementById('search-input').addEventListener('input', e => {
  const kw = e.target.value.toLowerCase();
  document.querySelectorAll('#config-list-body tr').forEach(tr => {
    const text = tr.textContent.toLowerCase();
    tr.style.display = text.includes(kw) ? '' : 'none';
  });
});

function editConfig(name, namespace) {
  editingName = name;
  ConfigAPI.get(name, namespace).then(data => {
    currentConfig = data.config;
    document.getElementById('edit-name').value = data.config.name;
    document.getElementById('edit-namespace').value = data.config.namespace;
    document.getElementById('edit-format').value = data.config.format;
    document.getElementById('edit-desc').value = data.config.description || '';
    document.getElementById('edit-content').value = data.content || '';
    return ConfigAPI.versions(name, namespace);
  }).then(versions => {
    editingVersions = versions || [];
    renderVersions(versions);
    switchPage('edit');
  }).catch(err => alert('加载失败: ' + err.message));
}

function deleteConfig(name, namespace) {
  if (!confirm(`确定删除配置 "${name}" 吗？此操作不可恢复。`)) return;
  ConfigAPI.del(name, namespace).then(() => loadConfigList()).catch(err => alert('删除失败: ' + err.message));
}

function renderVersions(versions) {
  const ul = document.getElementById('version-list');
  if (!versions || versions.length === 0) {
    ul.innerHTML = '<li class="empty">暂无版本</li>';
    return;
  }
  ul.innerHTML = versions.map(v => {
    const statusClass = v.status === 1 ? 'published' : v.status === 2 ? 'rollback' : 'draft';
    const statusText = v.status === 1 ? '已发布' : v.status === 2 ? '已回滚' : '草稿';
    return `
      <li onclick="selectVersion(${v.id})">
        <span class="ver-no">v${v.version}</span>
        <span class="ver-status ${statusClass}">${statusText}</span>
        <div style="margin-top:4px;color:#888;font-size:12px;">${escapeHtml(v.created_by)} · ${fmtTime(v.created_at)}</div>
        ${v.status === 0 ? `<button class="btn btn-sm btn-success" style="margin-top:6px;" onclick="event.stopPropagation();publishVersion(${v.id})">发布</button>` : ''}
        ${v.status === 1 ? `<button class="btn btn-sm btn-danger" style="margin-top:6px;" onclick="event.stopPropagation();rollbackVersion(${v.id})">回滚</button>` : ''}
      </li>
    `;
  }).join('');
}

function selectVersion(versionId) {
  const v = editingVersions.find(x => x.id === versionId);
  if (v) {
    document.getElementById('edit-content').value = v.content;
  }
}

function publishVersion(versionId) {
  if (!confirm('确认发布此版本？')) return;
  ConfigAPI.publish(editingName, { namespace: currentConfig.namespace, version_id: versionId })
    .then(() => {
      alert('发布成功');
      return ConfigAPI.versions(editingName, currentConfig.namespace);
    }).then(versions => {
      editingVersions = versions;
      renderVersions(versions);
    }).catch(err => alert('发布失败: ' + err.message));
}

function rollbackVersion(versionId) {
  if (!confirm('确认回滚到此版本？将基于该版本内容创建新版本。')) return;
  ConfigAPI.rollback(editingName, { namespace: currentConfig.namespace, version_id: versionId })
    .then(() => {
      alert('回滚成功');
      return ConfigAPI.versions(editingName, currentConfig.namespace);
    }).then(versions => {
      editingVersions = versions;
      renderVersions(versions);
    }).catch(err => alert('回滚失败: ' + err.message));
}

// ===== 编辑页操作 =====
document.getElementById('btn-cancel').addEventListener('click', () => switchPage('list'));

document.getElementById('btn-save-draft').addEventListener('click', () => {
  saveConfig(false);
});

document.getElementById('btn-publish').addEventListener('click', () => {
  saveConfig(true);
});

function saveConfig(shouldPublish) {
  const name = document.getElementById('edit-name').value.trim();
  const namespace = document.getElementById('edit-namespace').value.trim() || 'default';
  const format = document.getElementById('edit-format').value;
  const description = document.getElementById('edit-desc').value.trim();
  const content = document.getElementById('edit-content').value;
  if (!name) { alert('配置名称不能为空'); return; }
  if (!content) { alert('配置内容不能为空'); return; }

  const body = { name, namespace, format, description, content };

  if (editingName) {
    ConfigAPI.update(editingName, body).then(data => {
      if (shouldPublish && data.version_id) {
        return ConfigAPI.publish(editingName, { namespace, version_id: data.version_id });
      }
    }).then(() => {
      alert(shouldPublish ? '保存并发布成功' : '保存草稿成功');
      switchPage('list');
    }).catch(err => alert('保存失败: ' + err.message));
  } else {
    ConfigAPI.create(body).then(data => {
      if (shouldPublish && data.version_id) {
        return ConfigAPI.publish(name, { namespace, version_id: data.version_id });
      }
    }).then(() => {
      alert(shouldPublish ? '创建并发布成功' : '创建草稿成功');
      switchPage('list');
    }).catch(err => alert('创建失败: ' + err.message));
  }
}

// ===== 日志页 =====
function loadLogs() {
  const tbody = document.getElementById('log-list-body');
  tbody.innerHTML = '<tr><td colspan="6" class="empty">加载中...</td></tr>';
  ConfigAPI.allLogs().then(logs => {
    if (!logs || logs.length === 0) {
      tbody.innerHTML = '<tr><td colspan="6" class="empty">暂无日志</td></tr>';
      return;
    }
    tbody.innerHTML = logs.map(log => `
      <tr>
        <td>${fmtTime(log.created_at)}</td>
        <td>${escapeHtml(log.config_id)}</td>
        <td><span class="tag ${actionTagClass(log.action)}">${escapeHtml(log.action)}</span></td>
        <td>${log.version_id || '-'}</td>
        <td>${escapeHtml(log.operator)}</td>
        <td style="max-width:300px;overflow:hidden;text-overflow:ellipsis;">${escapeHtml(log.detail || '-')}</td>
      </tr>
    `).join('');
  }).catch(err => {
    tbody.innerHTML = `<tr><td colspan="6" class="empty">加载失败: ${escapeHtml(err.message)}</td></tr>`;
  });
}

document.getElementById('log-filter').addEventListener('input', e => {
  const kw = e.target.value.toLowerCase();
  document.querySelectorAll('#log-list-body tr').forEach(tr => {
    tr.style.display = tr.textContent.toLowerCase().includes(kw) ? '' : 'none';
  });
});

// ===== 工具函数 =====
function escapeHtml(str) {
  if (str == null) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function fmtTime(ts) {
  if (!ts) return '-';
  const d = new Date(ts * 1000);
  return d.toLocaleString('zh-CN');
}

function statusTag(s) {
  if (s === 0) return '<span class="tag tag-green">正常</span>';
  if (s === 1) return '<span class="tag tag-red">禁用</span>';
  return '<span class="tag">未知</span>';
}

function actionTagClass(action) {
  const map = {
    create: 'tag-green', publish: 'tag-green', edit: 'tag-orange',
    rollback: 'tag-orange', delete: 'tag-red'
  };
  return map[action] || '';
}

function closeModal() {
  document.getElementById('modal-publish').classList.remove('show');
}

// 初始化
loadConfigList();
