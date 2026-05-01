const API_BASE = 'http://127.0.0.1:8087';

function api(path, opts = {}) {
  const url = API_BASE + path;
  const headers = {
    'Content-Type': 'application/json',
    'X-Operator': 'admin',
    ...(opts.headers || {})
  };
  return fetch(url, { ...opts, headers })
    .then(r => r.json())
    .then(j => {
      if (!j.ok) throw new Error(j.msg || '请求失败');
      return j.data;
    });
}

const ConfigAPI = {
  list(namespace = 'default') {
    return api(`/api/configs?namespace=${namespace}`);
  },
  get(name, namespace = 'default') {
    return api(`/api/configs/${name}?namespace=${namespace}`);
  },
  create(body) {
    return api('/api/configs', { method: 'POST', body: JSON.stringify(body) });
  },
  update(name, body) {
    return api(`/api/configs/${name}`, { method: 'PUT', body: JSON.stringify(body) });
  },
  del(name, namespace = 'default') {
    return api(`/api/configs/${name}?namespace=${namespace}`, { method: 'DELETE' });
  },
  publish(name, body) {
    return api(`/api/configs/${name}/publish`, { method: 'POST', body: JSON.stringify(body) });
  },
  rollback(name, body) {
    return api(`/api/configs/${name}/rollback`, { method: 'POST', body: JSON.stringify(body) });
  },
  versions(name, namespace = 'default') {
    return api(`/api/configs/${name}/versions?namespace=${namespace}`);
  },
  logs(name, namespace = 'default', limit = 100) {
    return api(`/api/configs/${name}/logs?namespace=${namespace}&limit=${limit}`);
  },
  allLogs(limit = 200) {
    // 由于没有全局日志接口，先聚合所有配置的日志（简化实现）
    return this.list().then(list => {
      const promises = list.map(c => this.logs(c.name, c.namespace, 50).catch(() => []));
      return Promise.all(promises).then(results =>
        results.flat().sort((a, b) => b.created_at - a.created_at).slice(0, limit)
      );
    });
  }
};
