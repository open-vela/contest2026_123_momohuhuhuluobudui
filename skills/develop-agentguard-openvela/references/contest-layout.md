# Contest repository contract

- Develop on `dev-ai-contest-2026`.
- Put owned code under this repository only: `app/`, `quickapp/`, `board/`,
  `monitor/`, `skills/`, `docs/`, and `logs/` as applicable.
- Map an application into the outer build tree with a manifest `linkfile`, for
  example:

  ```xml
  <linkfile src="app/agentguard"
            dest="packages/demos/contest2026_123_agentguard"/>
  ```

- Do not copy or patch public `nuttx`, `apps`, `packages`, or `vendor` sources
  into the contest commit. Submit necessary public changes through their own
  upstream repositories.
- Build from the openvela root, not from the contest repository.
- Preserve `logs/`; exported AI Coding logs are part of the submission.
- Replace the scaffold root README with the work description before submission.
- Never commit Wi-Fi passwords, shared tokens, private keys, personal IP
  addresses, or generated firmware/build outputs.
