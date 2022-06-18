import { SiCsharp } from "react-icons/si";
import { Feature } from 'shared/api.types';
import { StatusState } from 'store/StatusState';
import { ResourceTemplateDescriptor } from '../types';

export default {
  id: 'csharp',
  icon: <SiCsharp />,
  title: 'C#',
  description: 'Easy start C# resource template',

  useIsEnabled: () => StatusState.getFeature(Feature.dotnetAvailable),
  disabledDescription: '.NET is not installed on your system, please install it and restart FxDK to get this template available',
} as ResourceTemplateDescriptor;
