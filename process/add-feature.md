This is the process to follow when adding a feature

```mermaid
graph TD;
    A[I Want to Add a Feature] -->|Link it to Epic & Milestone| B[Create Issue from Template]
    B --> C[Create Branch from GitHub]
    C --> D[Code the Feature]
    D -->|Feature Works?| E{Test Passed?}
    E -->|No| G[Fix Issues Before Push] --> D
    E -->|Yes| H{Update Documentation?}
    H -->|Yes| I[Update Documentation] --> J[Push it]
    H -->|No| J[Push it]
    J --> L{Need to Integrate Code in Another Branch?}
    L -->|Yes| M[Create Merging Branch or Merge on Your Own Branch, and test it] --> K[Create PR from Template]
    L -->|No| K[Create PR from Template]
```
