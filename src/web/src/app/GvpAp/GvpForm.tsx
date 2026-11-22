import * as React from 'react';
import {
  Gallery,
  GalleryItem,
  FormGroup,
  FormSelect,
  FormSelectOption,
  InputGroup,
  InputGroupText,
  TextInput,
  TextArea,
  Button,
  Modal,
  Alert,
} from '@patternfly/react-core';
import { InquiredFrequencyForm } from '../RatAfc/InquiredFrequencyForm';
import {
  DeploymentType,
  ExclusionZoneInquiryRequest,
  FrequencyRange,
  GVP_PRECISION_VALUES,
  GVP_RULESET_ID,
  Location,
} from '../Lib/GvpTypes';

/**
 * GvpForm.tsx: Form for one GVP Exclusion Zone Inquiry
 */

interface GvpFormState {
  serialNumber: string;
  certificationId: string;
  shape: 'ellipse' | 'circle';
  latitude?: number;
  longitude?: number;
  majorAxis?: number;
  minorAxis?: number;
  orientation?: number;
  radius?: number;
  indoorDeployment?: DeploymentType;
  inquiredFrequencyRange: FrequencyRange[];
  psdLevels: string;
  desiredPrecision?: string;
}

const emptyForm: GvpFormState = {
  serialNumber: '',
  certificationId: '',
  shape: 'ellipse',
  inquiredFrequencyRange: [],
  psdLevels: '',
};

const NumberField: React.FunctionComponent<{
  id: string;
  label: string;
  unit: string;
  value?: number;
  isValid: (n: number) => boolean;
  onChange: (n?: number) => void;
}> = (props) => (
  <GalleryItem>
    <FormGroup label={props.label} fieldId={props.id}>
      <InputGroup>
        <TextInput
          id={props.id}
          type="number"
          step="any"
          value={props.value === undefined ? '' : props.value}
          isValid={props.value === undefined || props.isValid(props.value)}
          onChange={(x) => props.onChange(x === '' ? undefined : Number(x))}
          style={{ textAlign: 'right' }}
        />
        <InputGroupText>{props.unit}</InputGroupText>
      </InputGroup>
    </FormGroup>
  </GalleryItem>
);

// Unfilled values serialize as absent, so the server reports them as missing parameters.
const toLocation = (form: GvpFormState): Location => {
  const center = { latitude: form.latitude!, longitude: form.longitude! };
  const area: Location =
    form.shape === 'ellipse'
      ? { ellipse: { center, majorAxis: form.majorAxis!, minorAxis: form.minorAxis!, orientation: form.orientation! } }
      : { circle: { ...center, radius: form.radius! } };
  return { ...area, indoorDeployment: form.indoorDeployment };
};

// Empty optional fields are left out, per SDI section 3.2.
const toRequest = (form: GvpFormState): ExclusionZoneInquiryRequest => {
  const psd = form.psdLevels
    .split(',')
    .map((x) => x.trim())
    .filter((x) => x.length > 0)
    .map(Number);
  return {
    requestId: '1',
    deviceDescriptor: {
      serialNumber: form.serialNumber,
      certificationId: [{ rulesetId: GVP_RULESET_ID, id: form.certificationId }],
    },
    areaOfIntendedOperation: toLocation(form),
    inquiredFrequencyRange: form.inquiredFrequencyRange.length > 0 ? form.inquiredFrequencyRange : undefined,
    desiredPsd: psd.length > 0 ? psd : undefined,
    desiredPrecision: form.desiredPrecision,
  };
};

const fromRequest = (request: ExclusionZoneInquiryRequest): GvpFormState => {
  const { ellipse, circle, indoorDeployment } = request.areaOfIntendedOperation;
  return {
    serialNumber: request.deviceDescriptor?.serialNumber || '',
    certificationId: request.deviceDescriptor?.certificationId?.[0]?.id || '',
    shape: circle ? 'circle' : 'ellipse',
    latitude: ellipse ? ellipse.center?.latitude : circle?.latitude,
    longitude: ellipse ? ellipse.center?.longitude : circle?.longitude,
    majorAxis: ellipse?.majorAxis,
    minorAxis: ellipse?.minorAxis,
    orientation: ellipse?.orientation,
    radius: circle?.radius,
    indoorDeployment: indoorDeployment,
    inquiredFrequencyRange: request.inquiredFrequencyRange || [],
    psdLevels: (request.desiredPsd || []).join(', '),
    desiredPrecision: request.desiredPrecision,
  };
};

const parsePaste = (text: string): ExclusionZoneInquiryRequest => {
  const json = JSON.parse(text);
  const request = Array.isArray(json.exclusionZoneInquiryRequests) ? json.exclusionZoneInquiryRequests[0] : json;
  if (!request || !request.areaOfIntendedOperation) {
    throw new Error('Expected an ExclusionZoneInquiryRequest or a message holding one');
  }
  if (!request.areaOfIntendedOperation.ellipse && !request.areaOfIntendedOperation.circle) {
    throw new Error('The form takes an ellipse or a circle');
  }
  return request;
};

export const GvpForm: React.FunctionComponent<{
  isSubmitting: boolean;
  onSubmit: (request: ExclusionZoneInquiryRequest) => void;
}> = (props) => {
  const [form, setForm] = React.useState<GvpFormState>(emptyForm);
  const update = (change: Partial<GvpFormState>) => setForm({ ...form, ...change });
  const [isPasteOpen, setIsPasteOpen] = React.useState(false);
  const [pasteText, setPasteText] = React.useState('');
  const [pasteError, setPasteError] = React.useState('');

  const openPaste = () => {
    setPasteText(JSON.stringify(toRequest(form), null, 2));
    setPasteError('');
    setIsPasteOpen(true);
  };

  const applyPaste = () => {
    try {
      setForm(fromRequest(parsePaste(pasteText)));
      setIsPasteOpen(false);
    } catch (e) {
      setPasteError(e.message);
    }
  };

  return (
    <>
      <Gallery gutter="sm">
        <GalleryItem>
          <FormGroup label="Serial Number" fieldId="gvp-serial-number">
            <TextInput
              id="gvp-serial-number"
              value={form.serialNumber}
              onChange={(x) => update({ serialNumber: x })}
              isValid={form.serialNumber.length > 0}
            />
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Certification Id" fieldId="gvp-certification-id">
            <TextInput
              id="gvp-certification-id"
              value={form.certificationId}
              onChange={(x) => update({ certificationId: x })}
              isValid={form.certificationId.length > 0}
            />
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Area of Intended Operation" fieldId="gvp-shape">
            <FormSelect
              id="gvp-shape"
              value={form.shape}
              onChange={(x: string) => update({ shape: x as GvpFormState['shape'] })}
            >
              <FormSelectOption value="ellipse" label="Ellipse" />
              <FormSelectOption value="circle" label="Circle" />
            </FormSelect>
          </FormGroup>
        </GalleryItem>
        <NumberField
          id="gvp-latitude"
          label="Latitude"
          unit="degrees"
          value={form.latitude}
          isValid={(n) => n >= -90 && n <= 90}
          onChange={(n) => update({ latitude: n })}
        />
        <NumberField
          id="gvp-longitude"
          label="Longitude"
          unit="degrees"
          value={form.longitude}
          isValid={(n) => n >= -180 && n <= 180}
          onChange={(n) => update({ longitude: n })}
        />
        {form.shape === 'ellipse' ? (
          <>
            <NumberField
              id="gvp-semi-major-axis"
              label="Semi-major Axis"
              unit="meters"
              value={form.majorAxis}
              isValid={(n) => n > 0}
              onChange={(n) => update({ majorAxis: n })}
            />
            <NumberField
              id="gvp-semi-minor-axis"
              label="Semi-minor Axis"
              unit="meters"
              value={form.minorAxis}
              isValid={(n) => n > 0}
              onChange={(n) => update({ minorAxis: n })}
            />
            <NumberField
              id="gvp-orientation"
              label="Orientation"
              unit="degrees"
              value={form.orientation}
              isValid={(n) => n >= 0 && n <= 180}
              onChange={(n) => update({ orientation: n })}
            />
          </>
        ) : (
          <NumberField
            id="gvp-radius"
            label="Radius"
            unit="meters"
            value={form.radius}
            isValid={(n) => n > 0}
            onChange={(n) => update({ radius: n })}
          />
        )}
        <GalleryItem>
          <FormGroup label="Indoor Deployment" fieldId="gvp-indoor-deployment">
            <FormSelect
              id="gvp-indoor-deployment"
              value={form.indoorDeployment === undefined ? '' : form.indoorDeployment}
              onChange={(x: string) => update({ indoorDeployment: x === '' ? undefined : Number(x) })}
            >
              <FormSelectOption value="" label="Not specified" />
              <FormSelectOption value={DeploymentType.Indoor} label="Indoor" />
              <FormSelectOption value={DeploymentType.Outdoor} label="Outdoor" />
              <FormSelectOption value={DeploymentType.Unknown} label="Unknown" />
            </FormSelect>
          </FormGroup>
        </GalleryItem>
        <InquiredFrequencyForm inquiredFrequencyRange={form.inquiredFrequencyRange} onChange={(x) => update(x)} />
        <GalleryItem>
          <FormGroup label="PSD Levels (comma separated)" fieldId="gvp-psd-levels">
            <InputGroup>
              <TextInput
                id="gvp-psd-levels"
                value={form.psdLevels}
                onChange={(x) => update({ psdLevels: x })}
                style={{ textAlign: 'right' }}
              />
              <InputGroupText>dBm/MHz</InputGroupText>
            </InputGroup>
          </FormGroup>
        </GalleryItem>
        <GalleryItem>
          <FormGroup label="Precision" fieldId="gvp-precision">
            <FormSelect
              id="gvp-precision"
              value={form.desiredPrecision || ''}
              onChange={(x: string) => update({ desiredPrecision: x === '' ? undefined : x })}
            >
              <FormSelectOption value="" label="Not specified" />
              {GVP_PRECISION_VALUES.map((p) => (
                <FormSelectOption key={p} value={p} label={p} />
              ))}
            </FormSelect>
          </FormGroup>
        </GalleryItem>
      </Gallery>
      <br />
      <Button variant="primary" isDisabled={props.isSubmitting} onClick={() => props.onSubmit(toRequest(form))}>
        Send Request
      </Button>{' '}
      <Button variant="secondary" onClick={openPaste}>
        Copy/Paste
      </Button>
      <Modal
        title="Copy/Paste"
        isLarge={true}
        isOpen={isPasteOpen}
        onClose={() => setIsPasteOpen(false)}
        actions={[
          <Button key="paste-close" variant="secondary" onClick={() => setIsPasteOpen(false)}>
            Close
          </Button>,
          <Button key="paste-apply" variant="primary" onClick={applyPaste}>
            Apply to form
          </Button>,
        ]}
      >
        <TextArea
          aria-label="Exclusion Zone Inquiry Request"
          resizeOrientation="vertical"
          rows={20}
          value={pasteText}
          onChange={(text: string) => setPasteText(text)}
        />
        {pasteError.length > 0 && <Alert variant="danger" isInline={true} title={pasteError} />}
      </Modal>
    </>
  );
};
