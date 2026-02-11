import type { AppSettings } from "../../../shared/settings";

export interface SettingsSectionProps {
  settings: AppSettings;
  onChange: (next: AppSettings) => void;
}
