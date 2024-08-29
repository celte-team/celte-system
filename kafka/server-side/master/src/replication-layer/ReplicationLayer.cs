class ReplicationLayer {
    private ReplicationLayer _replicationLayer;

    public ReplicationLayer(ReplicationLayer replicationLayer) {
        _replicationLayer = replicationLayer;
    }

    public void Replicate() {
        _replicationLayer.Replicate();
    }

    public void Stop() {
        _replicationLayer.Stop();
    }

    public void Start() {
        _replicationLayer.Start();
    }

}